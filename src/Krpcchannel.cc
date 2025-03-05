#include "Krpcchannel.h"
#include "Krpcheader.pb.h"
#include "zookeeperutil.h"
#include "Krpcapplication.h"
#include "Krpccontroller.h"
#include "memory"
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "KrpcLogger.h"
#include <chrono>
#include <thread>

std::mutex g_data_mutx;
// header_size+hservice_name method_name args_size+args_str
//  这里的callmethod是给客户端stub代理进行统一做rpc方法用的数据格式序列化喝网络发送，
//  和服务端的callmethod区分服务端的call method只是通过动态多态调用到客户端请求的方法。

void KrpcChannel::CallMethod(const ::google::protobuf::MethodDescriptor *method,
                             ::google::protobuf::RpcController *controller,
                             const ::google::protobuf::Message *request,
                             ::google::protobuf::Message *response,
                             ::google::protobuf::Closure *done)
{
    auto start_time = std::chrono::high_resolution_clock::now();

    LOG(INFO) << "=== 开始 RPC 调用 ===";
    LOG(INFO) << "调用服务: " << method->service()->name();
    LOG(INFO) << "调用方法: " << method->name();

    if (!m_connection || m_connection->fd == -1) {
        const google::protobuf::ServiceDescriptor *sd = method->service();
        service_name = sd->name();
        method_name = method->name();
        
        ZkClient zkCli;
        zkCli.Start();
        std::string host_data = QueryServiceHost(&zkCli, service_name, method_name, m_idx);
        m_ip = host_data.substr(0, m_idx);
        LOG(INFO) << "ip: " << m_ip;
        m_port = atoi(host_data.substr(m_idx + 1, host_data.size() - m_idx).c_str());
        LOG(INFO) << "port: " << m_port;

        m_connection = KrpcConnectionPool::GetInstance().GetConnection(m_ip, m_port);
        if (!m_connection) {
            LOG(ERROR) << "Failed to get connection from pool";
            controller->SetFailed("Failed to get connection from pool");
            return;
        }
    }

    uint32_t args_size = 0;
    std::string args_str;
    if (request->SerializeToString(&args_str)) {
        args_size = args_str.size();
    } else {
        controller->SetFailed("serialize request fail");
        return;
    }

    Krpc::RpcHeader krpcheader;
    krpcheader.set_service_name(service_name);
    krpcheader.set_method_name(method_name);
    krpcheader.set_args_size(args_size);

    uint32_t header_size = 0;
    std::string rpc_header_str;
    if (krpcheader.SerializeToString(&rpc_header_str)) {
        header_size = rpc_header_str.size();
    } else {
        controller->SetFailed("serialize rpc header error!");
        return;
    }

    std::string send_rpc_str;
    {
        google::protobuf::io::StringOutputStream string_output(&send_rpc_str);
        google::protobuf::io::CodedOutputStream coded_output(&string_output);
        coded_output.WriteVarint32(static_cast<uint32_t>(header_size));
        coded_output.WriteString(rpc_header_str);
    }
    send_rpc_str += args_str;

    LOG(INFO) << "准备发送请求，数据大小: " << send_rpc_str.size();
    
    if (-1 == send(m_connection->fd, send_rpc_str.c_str(), send_rpc_str.size(), 0)) {
        LOG(ERROR) << "发送失败，错误: " << strerror(errno);
        KrpcConnectionPool::GetInstance().CloseConnection(m_connection);
        m_connection = nullptr;
        controller->SetFailed(strerror(errno));
        return;
    }
    
    LOG(INFO) << "请求发送成功";
    
    char recv_buf[1024] = {0};
    int recv_size = 0;
    if (-1 == (recv_size = recv(m_connection->fd, recv_buf, 1024, 0))) {
        KrpcConnectionPool::GetInstance().CloseConnection(m_connection);
        m_connection = nullptr;
        controller->SetFailed(strerror(errno));
        return;
    }

    if (!response->ParseFromArray(recv_buf, recv_size)) {
        KrpcConnectionPool::GetInstance().CloseConnection(m_connection);
        m_connection = nullptr;
        controller->SetFailed("parse response error!");
        return;
    }

    // Return connection to pool
    KrpcConnectionPool::GetInstance().ReleaseConnection(m_connection);

    LOG(INFO) << "收到响应，数据大小: " << recv_size;
    LOG(INFO) << "=== RPC 调用结束 ===";

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    LOG(INFO) << "RPC 调用耗时: " << duration.count() << "ms";
}

bool KrpcChannel::newConnect(const char *ip, uint16_t port) {
    m_connection = KrpcConnectionPool::GetInstance().GetConnection(ip, port);
    return m_connection != nullptr;
}

std::string KrpcChannel::QueryServiceHost(ZkClient *zkclient, std::string service_name, std::string method_name, int& idx) {
    std::string method_path = "/" + service_name + "/" + method_name;
    LOG(INFO) << "method_path: " << method_path;
    std::unique_lock<std::mutex> lock(g_data_mutx);
    std::string host_data_1 = zkclient->GetData(method_path.c_str());
    lock.unlock(); 
    if (host_data_1 == "") {
        LOG(ERROR) << method_path + " is not exist!";
        return " ";
    }
    idx = host_data_1.find(":"); // 127.0.0.1:8000 获取到ip和port的分割符
    if (idx == -1) {
        LOG(ERROR) << method_path + " address is invalid!";
        return " ";
    }
    return host_data_1;
}

KrpcChannel::KrpcChannel(bool connectNow) : m_connection(nullptr), m_idx(0) {
    if (!connectNow) {
        return;
    }
    auto rt = newConnect(m_ip.c_str(), m_port);
    int count = 3;
    while (!rt && count--) {
        rt = newConnect(m_ip.c_str(), m_port);
    }
}

bool KrpcChannel::CallMethodWithRetry(const ::google::protobuf::MethodDescriptor *method,
                             ::google::protobuf::RpcController *controller,
                             const ::google::protobuf::Message *request,
                             ::google::protobuf::Message *response,
                             ::google::protobuf::Closure *done,
                             int max_retries)
{
    for (int i = 0; i < max_retries; i++) {
        try {
            LOG(INFO) << "尝试第 " << (i + 1) << " 次调用";
            CallMethod(method, controller, request, response, done);
            return true;
        } catch (const std::exception& e) {
            LOG(ERROR) << "调用失败: " << e.what();
            if (i < max_retries - 1) {
                LOG(INFO) << "等待重试...";
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }
    return false;
}

std::string KrpcChannel::SelectServer(const std::vector<std::string>& servers) {
    static int current = 0;
    current = (current + 1) % servers.size();
    return servers[current];
}