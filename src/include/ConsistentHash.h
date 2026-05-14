#ifndef CONSISTENT_HASH_H
#define CONSISTENT_HASH_H

#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <shared_mutex>
#include <atomic> // C++11 支持原子操作
#include <thread> // C++11 支持线程
#include <functional>
#include <cmath>
#include <mutex>   
// 一致性哈希配置
struct HashConfig
{
    int replicas;             // 初始每个节点的虚拟节点数
    int min_replicas;         // 最小虚拟节点数限制
    int max_replicas;         // 最大虚拟节点数限制
    double balance_threshold; // 负载不均阈值 (例如 0.25)
};

class ConsistentHash
{
public:
    // 构造函数
    ConsistentHash(HashConfig cfg = {10, 5, 200, 0.25});

    // 析构函数
    ~ConsistentHash();

    // 添加节点
    void AddNodes(const std::vector<std::string> &nodes);

    // 移除节点
    void RemoveNode(const std::string &node);

    // 根据 Key 获取节点
    std::string GetTargetNode(const std::string &key);

    void UpdateNodes(const std::vector<std::string> &nodes);
    // 获取统计信息
    std::unordered_map<std::string, double> GetStats();

private:
    // CRC32 哈希
    uint32_t hash_func(const std::string &data);

    // 内部逻辑
    void add_node_internal(const std::string &node, int replicas);
    void start_balancer();
    void check_and_rebalance();
    void rebalance_logic();

private:
    HashConfig m_config;

    std::vector<uint32_t> m_keys;
    std::unordered_map<uint32_t, std::string> m_ring;

    std::unordered_map<std::string, int> m_node_replicas;
    std::atomic<long long> m_total_requests;

    std::unordered_map<std::string, std::shared_ptr<std::atomic<long long>>> m_node_counts;
    mutable std::shared_timed_mutex m_rw_mtx;
    std::thread m_balancer_thread;
    std::atomic<bool> m_stop_balancer;
};

#endif