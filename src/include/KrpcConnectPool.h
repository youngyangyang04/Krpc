#ifndef KRPC_CONNECT_POOL_H
#define KRPC_CONNECT_POOL_H

#include <string>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <memory>
#include "LockFreeQueue.h" // 引用之前写的无锁队列

struct ConnectionBucket
{
    // 使用无锁队列存放空闲的 fd
    std::unique_ptr<MPMCQueue<int>> free_fds;
    // 记录该地址当前已建立的连接总数（包括正在使用的和空闲的）
    std::atomic<int> active_count; 

    ConnectionBucket(size_t capacity) 
        : free_fds(new MPMCQueue<int>(capacity)), active_count(0) {}
};

class KrpcConnectPool
{
public:
    static KrpcConnectPool &GetInstance();

    // 借用连接
    int BorrowConnection(const std::string &ip, uint16_t port);

    // 归还连接
    void ReturnConnection(const std::string &ip, uint16_t port, int fd, bool is_bad = false);

    // 预热连接
    void WarmUp(const std::string &ip, uint16_t port, int count);

private:
    KrpcConnectPool();
    ~KrpcConnectPool();

    int m_max_conn_per_node;
    // 全局 Bucket 映射表
    std::unordered_map<std::string, ConnectionBucket *> m_pools;
    std::mutex m_global_mtx; // 仅用于创建新 Bucket 时加锁
};

#endif