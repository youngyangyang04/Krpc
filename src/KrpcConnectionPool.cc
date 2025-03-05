#include "KrpcConnectionPool.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <thread>

KrpcConnectionPool::KrpcConnectionPool(size_t max_size, std::chrono::seconds max_idle_time)
    : m_max_size(max_size), m_max_idle_time(max_idle_time), m_running(true) {
    // Start the cleanup thread
    std::thread cleanup_thread([this]() {
        while (m_running) {
            CleanIdleConnections();
            std::this_thread::sleep_for(std::chrono::seconds(30)); // Check every 30 seconds
        }
    });
    cleanup_thread.detach();
}

KrpcConnectionPool::~KrpcConnectionPool() {
    m_running = false;
    std::lock_guard<std::mutex> lock(m_mutex);
    while (!m_connections.empty()) {
        auto conn = m_connections.front();
        if (conn->fd != -1) {
            close(conn->fd);
        }
        m_connections.pop();
    }
}

bool KrpcConnectionPool::CreateConnection(const std::string& ip, uint16_t port) {
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1) {
        LOG(ERROR) << "Create socket error: " << strerror(errno);
        return false;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());

    if (connect(clientfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        close(clientfd);
        LOG(ERROR) << "Connect error: " << strerror(errno);
        return false;
    }

    auto conn = std::make_shared<Connection>(clientfd, ip, port);
    m_connections.push(conn);
    return true;
}

std::shared_ptr<Connection> KrpcConnectionPool::GetConnection(const std::string& ip, uint16_t port) {
    std::unique_lock<std::mutex> lock(m_mutex);

    // Try to find an existing connection to the same ip:port
    std::queue<std::shared_ptr<Connection>> temp_queue;
    std::shared_ptr<Connection> result = nullptr;

    while (!m_connections.empty()) {
        auto conn = m_connections.front();
        m_connections.pop();

        if (!conn->in_use && conn->ip == ip && conn->port == port) {
            // Test if the connection is still alive
            char test;
            if (recv(conn->fd, &test, 1, MSG_PEEK | MSG_DONTWAIT) == 0) {
                // Connection is closed
                close(conn->fd);
                continue;
            }
            
            conn->in_use = true;
            conn->last_used = std::chrono::steady_clock::now();
            result = conn;
            break;
        }
        temp_queue.push(conn);
    }

    // Restore connections that weren't chosen
    while (!temp_queue.empty()) {
        m_connections.push(temp_queue.front());
        temp_queue.pop();
    }

    if (result) {
        return result;
    }

    // If no existing connection found and pool not full, create new one
    if (m_connections.size() < m_max_size) {
        if (CreateConnection(ip, port)) {
            auto conn = m_connections.back();
            conn->in_use = true;
            return conn;
        }
    }

    // Wait for a connection to become available
    auto timeout = std::chrono::seconds(5);
    m_cv.wait_for(lock, timeout, [this]() {
        return !m_connections.empty();
    });

    if (!m_connections.empty()) {
        auto conn = m_connections.front();
        m_connections.pop();
        conn->in_use = true;
        conn->last_used = std::chrono::steady_clock::now();
        return conn;
    }

    LOG(ERROR) << "Failed to get connection from pool";
    return nullptr;
}

void KrpcConnectionPool::ReleaseConnection(std::shared_ptr<Connection> conn) {
    if (!conn) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    conn->in_use = false;
    conn->last_used = std::chrono::steady_clock::now();
    m_connections.push(conn);
    m_cv.notify_one();
}

void KrpcConnectionPool::CloseConnection(std::shared_ptr<Connection> conn) {
    if (!conn) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    if (conn->fd != -1) {
        close(conn->fd);
        conn->fd = -1;
    }
}

void KrpcConnectionPool::CleanIdleConnections() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::queue<std::shared_ptr<Connection>> active_connections;
    auto now = std::chrono::steady_clock::now();

    while (!m_connections.empty()) {
        auto conn = m_connections.front();
        m_connections.pop();

        if (!conn->in_use) {
            auto idle_time = std::chrono::duration_cast<std::chrono::seconds>(
                now - conn->last_used);

            if (idle_time > m_max_idle_time) {
                if (conn->fd != -1) {
                    close(conn->fd);
                }
                continue;
            }
        }
        active_connections.push(conn);
    }

    m_connections = std::move(active_connections);
} 