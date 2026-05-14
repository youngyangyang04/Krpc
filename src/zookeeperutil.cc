#include "zookeeperutil.h"
#include "Krpcapplication.h"
#include <mutex>
#include "KrpcLogger.h"
#include <condition_variable>

std::mutex cv_mutex;
std::condition_variable cv;
bool is_connected = false;

void global_watcher(zhandle_t *zh, int type, int status, const char *path, void *watcherCtx) {
    if (type == ZOO_SESSION_EVENT) {
        if (status == ZOO_CONNECTED_STATE) {
            std::lock_guard<std::mutex> lock(cv_mutex);
            is_connected = true;
        }
    }
    cv.notify_all();
}

ZkClient::ZkClient() : m_zhandle(nullptr) {}

ZkClient::~ZkClient() {
    if (m_zhandle != nullptr) {
        zookeeper_close(m_zhandle);
    }
}

void ZkClient::Start() {
    std::string host = KrpcApplication::GetInstance().GetConfig().Load("zookeeperip");
    std::string port = KrpcApplication::GetInstance().GetConfig().Load("zookeeperport");
    std::string connstr = host + ":" + port;

    m_zhandle = zookeeper_init(connstr.c_str(), global_watcher, 6000, nullptr, nullptr, 0);
    if (nullptr == m_zhandle) {
        LOG(ERROR) << "zookeeper_init error";
        exit(EXIT_FAILURE);
    }

    std::unique_lock<std::mutex> lock(cv_mutex);
    cv.wait(lock, [] { return is_connected; });
    LOG(INFO) << "zookeeper_init success";
}

void ZkClient::Create(const char *path, const char *data, int datalen, int state) {
    char path_buffer[128];
    int bufferlen = sizeof(path_buffer);

    int flag = zoo_exists(m_zhandle, path, 0, nullptr);
    if (flag == ZNONODE) {
        flag = zoo_create(m_zhandle, path, data, datalen, &ZOO_OPEN_ACL_UNSAFE, state, path_buffer, bufferlen);
        if (flag == ZOK) {
            LOG(INFO) << "znode create success... path:" << path;
        } else {
            LOG(ERROR) << "znode create failed... path:" << path;
            exit(EXIT_FAILURE);
        }
    }
}

std::string ZkClient::GetData(const char *path) {
    char buf[64] = {0};
    int bufferlen = sizeof(buf);

    int flag = zoo_get(m_zhandle, path, 0, buf, &bufferlen, nullptr);
    if (flag != ZOK) {
        LOG(ERROR) << "zoo_get error";
        return "";
    }
    return buf;
}

std::vector<std::string> ZkClient::GetChildren(const char* path, watcher_fn fn, void* cbContext) {
    struct String_vector nodes;

    int flag = zoo_wget_children(m_zhandle, path, fn, cbContext, &nodes);
    
    std::vector<std::string> vec;
    if (flag == ZOK) {
        for (int i = 0; i < nodes.count; ++i) {
            std::string child_path = std::string(path);
            if (child_path.back() != '/') child_path += "/";
            child_path += nodes.data[i];
            
            std::string data = GetData(child_path.c_str());
            if (!data.empty()) {
                vec.push_back(data);
            }
        }
        deallocate_String_vector(&nodes);
    } else {
        LOG(ERROR) << "zoo_wget_children error, path: " << path;
    }
    
    return vec;
}