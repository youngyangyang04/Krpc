#ifndef _Krpcprovider_H__
#define _Krpcprovider_H__
#include "google/protobuf/service.h"
#include "zookeeperutil.h"
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/base/ThreadPool.h> // 【修改点1】：引入muduo线程池头文件
#include <google/protobuf/descriptor.h>
#include <functional>
#include <string>
#include <unordered_map>

class KrpcProvider
{
public:
    void NotifyService(google::protobuf::Service* service);
    ~KrpcProvider();
    void Run();
private:
    muduo::net::EventLoop event_loop;
    

    muduo::ThreadPool m_thread_pool; 

    struct ServiceInfo
    {
        google::protobuf::Service* service;
        std::unordered_map<std::string, const google::protobuf::MethodDescriptor*> method_map;
    };
    std::unordered_map<std::string, ServiceInfo>service_map;
    
    void OnConnection(const muduo::net::TcpConnectionPtr& conn);
    void OnMessage(const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buffer, muduo::Timestamp receive_time);
    
    void SendRpcResponse(const muduo::net::TcpConnectionPtr& conn, google::protobuf::Message* response, google::protobuf::Message* request);
};
#endif