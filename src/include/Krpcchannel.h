#ifndef _Krpcchannel_h_
#define _Krpcchannel_h_
// 此类是继承自google::protobuf::RpcChannel
// 目的是为了给客户端进行方法调用的时候，统一接收的
#include <google/protobuf/service.h>
#include "zookeeperutil.h"
#include "KrpcConnectionPool.h"
#include <vector>
#include <memory>

class KrpcChannel : public google::protobuf::RpcChannel
{
public:
    KrpcChannel(bool connectNow);
    virtual ~KrpcChannel()
    {
    }
    void CallMethod(const ::google::protobuf::MethodDescriptor *method,
                    ::google::protobuf::RpcController *controller,
                    const ::google::protobuf::Message *request,
                    ::google::protobuf::Message *response,
                    ::google::protobuf::Closure *done) override; // override可以验证是否是虚函数

    bool CallMethodWithRetry(const ::google::protobuf::MethodDescriptor *method,
                           ::google::protobuf::RpcController *controller,
                           const ::google::protobuf::Message *request,
                           ::google::protobuf::Message *response,
                           ::google::protobuf::Closure *done,
                           int max_retries = 3);

    std::string SelectServer(const std::vector<std::string>& servers);

private:
    std::shared_ptr<Connection> m_connection;
    std::string service_name;
    std::string m_ip;
    uint16_t m_port;
    std::string method_name;
    int m_idx; // 用来划分服务器ip和port的下标
    bool newConnect(const char *ip, uint16_t port);
    std::string QueryServiceHost(ZkClient *zkclient, std::string service_name, std::string method_name, int &idx);
};
#endif
