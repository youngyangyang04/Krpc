#include "ServiceDiscovery.h"
#include "KrpcLogger.h"

ServiceDiscovery &ServiceDiscovery::GetInstance()
{
    static ServiceDiscovery instance;
    return instance;
}

void ServiceDiscovery::Init()
{
    m_zkClient.Start();
}

std::string ServiceDiscovery::GetTargetNode(const std::string &service_name, const std::string &key)
{
    {
      
        std::shared_lock<std::shared_timed_mutex> read_lock(m_rw_mtx); 
        if (m_chash_map.find(service_name) != m_chash_map.end())
        {
            return m_chash_map[service_name]->GetTargetNode(key);
        }
    }

    // 如果还没有该服务，需要去 ZK 拉取（此时不能加锁，因为网络请求很慢）
    std::string path = "/" + service_name;
    std::vector<std::string> new_nodes = m_zkClient.GetChildren(path.c_str(), WatcherCallback, this);

    {
        // 拉取到数据后，加写锁（独占锁）更新内部数据结构
        std::unique_lock<std::shared_timed_mutex> write_lock(m_rw_mtx);
        // 双重检查，防止其他线程已经初始化
        if (m_chash_map.find(service_name) == m_chash_map.end())
        {
            m_chash_map[service_name] = std::unique_ptr<ConsistentHash>(new ConsistentHash());
            m_chash_map[service_name]->UpdateNodes(new_nodes);
            m_nodes_cache[service_name] = new_nodes;
        }
        return m_chash_map[service_name]->GetTargetNode(key);
    }
}

void ServiceDiscovery::WatcherCallback(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx)
{
    if (type == ZOO_CHILD_EVENT)
    { 
        ServiceDiscovery *sd = static_cast<ServiceDiscovery *>(watcherCtx);
        std::string path_str = path;
        std::string service_name = path_str.substr(1); 

        // 千万不要在这里加锁！先发起阻塞网络请求获取最新节点
        std::vector<std::string> new_nodes = sd->m_zkClient.GetChildren(path_str.c_str(), WatcherCallback, sd);
        
        // 拿到数据后，再加写锁更新内存结构
        std::unique_lock<std::shared_timed_mutex> lock(sd->m_rw_mtx);
        if (sd->m_chash_map.find(service_name) != sd->m_chash_map.end()) {
            sd->m_chash_map[service_name]->UpdateNodes(new_nodes);
        }
        sd->m_nodes_cache[service_name] = new_nodes;
    }
}

