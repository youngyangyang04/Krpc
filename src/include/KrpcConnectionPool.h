#ifndef _KrpcConnectionPool_H_
#define _KrpcConnectionPool_H_

#include <queue>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <chrono>
#include <string>
#include "KrpcLogger.h"

struct Connection {
    int fd;
    std::string ip;
    uint16_t port;
    std::chrono::steady_clock::time_point last_used;
    bool in_use;

    Connection(int fd = -1, const std::string& ip = "", uint16_t port = 0)
        : fd(fd), ip(ip), port(port), in_use(false) {
        last_used = std::chrono::steady_clock::now();
    }
};

class KrpcConnectionPool {
public:
    static KrpcConnectionPool& GetInstance() {
        static KrpcConnectionPool instance;
        return instance;
    }

    std::shared_ptr<Connection> GetConnection(const std::string& ip, uint16_t port);
    void ReleaseConnection(std::shared_ptr<Connection> conn);
    void CloseConnection(std::shared_ptr<Connection> conn);

private:
    KrpcConnectionPool(size_t max_size = 100, 
                      std::chrono::seconds max_idle_time = std::chrono::seconds(60));
    ~KrpcConnectionPool();

    bool CreateConnection(const std::string& ip, uint16_t port);
    void CleanIdleConnections();

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::queue<std::shared_ptr<Connection>> m_connections;
    size_t m_max_size;
    std::chrono::seconds m_max_idle_time;
    bool m_running;

    KrpcConnectionPool(const KrpcConnectionPool&) = delete;
    KrpcConnectionPool& operator=(const KrpcConnectionPool&) = delete;
};

#endif 