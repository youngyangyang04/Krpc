#ifndef _Krpcchannel_h_
#define _Krpcchannel_h_

#include <google/protobuf/service.h>
#include <unistd.h>
#include "ServiceDiscovery.h"

class KrpcChannel : public google::protobuf::RpcChannel
{
public:

    KrpcChannel(bool connectNow = false) {} 
    
    // 【关键修改点】：析构函数也直接实现
    virtual ~KrpcChannel() override {}

    void CallMethod(const ::google::protobuf::MethodDescriptor *method,
                    ::google::protobuf::RpcController *controller,
                    const ::google::protobuf::Message *request,
                    ::google::protobuf::Message *response,
                    ::google::protobuf::Closure *done) override;

private:
    // 内部辅助函数声明
    ssize_t recv_exact(int fd, char *buf, size_t size);
};

#endif