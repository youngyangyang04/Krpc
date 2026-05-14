#include "KrpcConnectPool.h"
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "KrpcLogger.h"

// 假设每个 IP:Port 最多建立 1024 个长连接（必须是 2 的幂）
static const int MAX_CONN_SIZE = 1024;

KrpcConnectPool &KrpcConnectPool::GetInstance() {
    static KrpcConnectPool instance;
    return instance;
}

KrpcConnectPool::KrpcConnectPool() : m_max_conn_per_node(MAX_CONN_SIZE) {}

KrpcConnectPool::~KrpcConnectPool() {
    std::lock_guard<std::mutex> lock(m_global_mtx);
    for (auto &pair : m_pools) {
        int fd;
        while (pair.second->free_fds->pop(fd)) {
            close(fd);
        }
        delete pair.second;
    }
}

// 【新增核心功能】：预热连接
void KrpcConnectPool::WarmUp(const std::string &ip, uint16_t port, int count) {
    std::string key = ip + ":" + std::to_string(port);
    ConnectionBucket *bucket = nullptr;

    // 1. 确保 Bucket 存在
    {
        std::lock_guard<std::mutex> lock(m_global_mtx);
        auto it = m_pools.find(key);
        if (it == m_pools.end()) {
            bucket = new ConnectionBucket(MAX_CONN_SIZE);
            m_pools[key] = bucket;
        } else {
            bucket = it->second;
        }
    }

    int success_count = 0;
    // 2. 循环建立连接
    for (int i = 0; i < count; ++i) {
        // 检查是否超过最大连接限制
        if (bucket->active_count.load() >= m_max_conn_per_node) break;

        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd == -1) continue;

        // 设置非阻塞/禁用Nagle
        int opt = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());

        // 执行握手
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            // 成功后，原子增加计数并推入无锁队列
            bucket->active_count.fetch_add(1);
            if (!bucket->free_fds->push(fd)) {
                // 队列若满（理论上由 active_count 控制不会发生），则回滚
                close(fd);
                bucket->active_count.fetch_sub(1);
            } else {
                success_count++;
            }
        } else {
            close(fd);
        }
    }
    LOG(INFO) << "[ConnectPool] WarmUp complete: " << success_count 
              << " connections ready for " << key;
}

int KrpcConnectPool::BorrowConnection(const std::string &ip, uint16_t port) {
    std::string key = ip + ":" + std::to_string(port);

    ConnectionBucket *bucket = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_global_mtx);
        auto it = m_pools.find(key);
        if (it == m_pools.end()) {
            bucket = new ConnectionBucket(MAX_CONN_SIZE);
            m_pools[key] = bucket;
        } else {
            bucket = it->second;
        }
    }

    int fd = -1;
    // 优先从无锁队列获取（WarmUp 过的连接会在这里被直接拿到）
    if (bucket->free_fds->pop(fd)) {
        return fd;
    }

    // 队列为空时才尝试动态创建
    if (bucket->active_count.load(std::memory_order_relaxed) < m_max_conn_per_node) {
        if (bucket->active_count.fetch_add(1) < m_max_conn_per_node) {
            fd = socket(AF_INET, SOCK_STREAM, 0);
            int opt = 1;
            setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = inet_addr(ip.c_str());

            if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
                return fd;
            } else {
                close(fd);
                bucket->active_count.fetch_sub(1);
                return -1;
            }
        } else {
            bucket->active_count.fetch_sub(1);
        }
    }
    return -1; 
}

void KrpcConnectPool::ReturnConnection(const std::string &ip, uint16_t port, int fd, bool is_bad) {
    std::string key = ip + ":" + std::to_string(port);
    
    ConnectionBucket *bucket = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_global_mtx);
        auto it = m_pools.find(key);
        if (it != m_pools.end()) bucket = it->second;
    }

    if (!bucket) {
        close(fd);
        return;
    }

    if (is_bad) {
        close(fd);
        bucket->active_count.fetch_sub(1);
    } else {
        if (!bucket->free_fds->push(fd)) {
            close(fd);
            bucket->active_count.fetch_sub(1);
        }
    }
}