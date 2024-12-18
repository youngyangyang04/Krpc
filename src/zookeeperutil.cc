#include "zookeeperutil.h"
#include "Krpcapplication.h"
#include <mutex>
#include "KrpcLogger.h"
#include <condition_variable>
std::mutex cv_mutex;        // 全局锁
std::condition_variable cv; // 信号量
bool is_connected = false;
// 全局的watcher观察器，zkserver给zkclient的通知
void global_watcher(zhandle_t *zh, int type, int status, const char *path, void *watcherCtx)
{
    if (type == ZOO_SESSION_EVENT) // 回调消息类型和会话相关的消息类型
    {
        if (status == ZOO_CONNECTED_STATE) // zkclient和zkserver连接成功
        {
            std::lock_guard<std::mutex> lock(cv_mutex); // 加锁保护
            is_connected = true;
        }
    }
    cv.notify_all();
}
ZkClient::ZkClient() : m_zhandle(nullptr)
{
}
ZkClient::~ZkClient()
{
    if (m_zhandle != nullptr)
    {
        zookeeper_close(m_zhandle);
    }
}
void ZkClient::Start()
{

    std::string host = KrpcApplication::GetInstance().GetConfig().Load("zookeeperip");   // 获取zookeeper服务端的ip
    std::string port = KrpcApplication::GetInstance().GetConfig().Load("zookeeperport"); // 获取zoo keeper服务端的port
    std::string connstr = host + ":" + port;
    /*
    zookeeper_mt：多线程版本
    zookeeper的API客户端程序提供了三个线程
    API调用线程
    网络I/O线程  pthread_create  poll
    watcher回调线程 pthread_create
    */
    // 使用zookeeper_init初始化一个zk对象，异步建立rpcserver和zkclient之间的连接
    m_zhandle = zookeeper_init(connstr.c_str(), global_watcher, 6000, nullptr, nullptr, 0);
    if (nullptr == m_zhandle)
    { // 这个返回值不代表连接成功或者不成功
        LOG(ERROR) << "zookeeper_init error";
        exit(EXIT_FAILURE);
    }

    std::unique_lock<std::mutex> lock(cv_mutex);
    cv.wait(lock,[]{return is_connected;});
    LOG(INFO) << "zookeeper_init success";
}
void ZkClient::Create(const char *path, const char *data, int datalen, int state)
{
    // 创建znode节点，可以选择永久性节点还是临时节点。
    char path_buffer[128];
    int bufferlen = sizeof(path_buffer);
    int flag = zoo_exists(m_zhandle, path, 0, nullptr);
    if (flag == ZNONODE) // 表示节点不存在
    {
        // 创建指定的path的znode节点
        flag = zoo_create(m_zhandle, path, data, datalen, &ZOO_OPEN_ACL_UNSAFE, state, path_buffer, bufferlen);
        if (flag == ZOK)
        {
            LOG(INFO) << "znode create success... path:" << path;
        }
        else
        {
            LOG(ERROR) << "znode create success... path:" << path;
            exit(EXIT_FAILURE);
        }
    }
}
std::string ZkClient::GetData(const char *path)
{
    char buf[64];
    int bufferlen = sizeof(buf);
    int flag = zoo_get(m_zhandle, path, 0, buf, &bufferlen, nullptr);
    if (flag != ZOK)
    {
        LOG(ERROR) << "zoo_get error";
        return "";
    }
    else
    {
        return buf;
    }
    return "";
}
