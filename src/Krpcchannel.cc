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
    if(-1==m_clientfd){
    //获取服务对象和方法名    
    const google::protobuf::ServiceDescriptor *sd = method->service();
    service_name = sd->name();
    method_name = method->name();
    // rpc调用方也就是客户端想要调用服务器上服务对象提供的方法，需要查询zk上该服务所在的host信息。
    ZkClient zkCli;
    zkCli.Start(); // start返回就代表成功连接上zk服务器了
    std::string host_data=QueryServiceHost(&zkCli, service_name, method_name,m_idx);
    m_ip= host_data.substr(0, m_idx);
    std::cout << "ip: " << m_ip << std::endl;
    m_port = atoi(host_data.substr(m_idx + 1, host_data.size() - m_idx).c_str());
    std::cout << "port: " << m_port << std::endl;
    auto rt=newConnect(m_ip.c_str(), m_port);
    if(!rt){
        LOG(ERROR)<<"connect server error";
        return;
    }else{
        LOG(INFO)<<"connect server success";
    }
    }//endif

    // 获取参数的序列化字符串长度args_size
    uint32_t args_size{};
    std::string args_str;
    if (request->SerializeToString(&args_str))
    {
        args_size = args_str.size();
    }
    else
    {
        controller->SetFailed("serialize request fail");
        return;
    }
    // 定义rpc的报文header
    Krpc::RpcHeader krpcheader;
    krpcheader.set_service_name(service_name);
    krpcheader.set_method_name(method_name);
    krpcheader.set_args_size(args_size);

    uint32_t header_size = 0;
    std::string rpc_header_str;
    if (krpcheader.SerializeToString(&rpc_header_str))
    {
        header_size = rpc_header_str.size();
    }
    else
    {
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
    //  打印调试信息
    // std::cout << "============================================" << std::endl;
    // std::cout << "header_size: " << header_size << std::endl;
    // std::cout << "rpc_header_str: " << rpc_header_str << std::endl;
    // std::cout << "service_name: " << service_name << std::endl;
    // std::cout << "method_name: " << method_name << std::endl;
    // std::cout << "args_str: " << args_str << std::endl;
    // std::cout << "============================================" << std::endl;

    // 发送rpc的请求
    if (-1 == send(m_clientfd, send_rpc_str.c_str(), send_rpc_str.size(), 0))
    {
        close(m_clientfd);
        char errtxt[512] = {};
        std::cout << "send error: " << strerror_r(errno, errtxt, sizeof(errtxt)) << std::endl;
        controller->SetFailed(errtxt);
        return;
    }
    // 接收rpc请求的响应值
    char recv_buf[1024] = {0};
    int recv_size = 0;
    if (-1 == (recv_size = recv(m_clientfd, recv_buf, 1024, 0)))
    {
        char errtxt[512] = {};
        std::cout << "recv error" << strerror_r(errno, errtxt, sizeof(errtxt)) << std::endl;
        controller->SetFailed(errtxt);
        return;
    }
    // 反序列化rpc调用响应数据
    if (!response->ParseFromArray(recv_buf, recv_size))
    {
        close(m_clientfd);
        char errtxt[512] = {};
        std::cout << "parese error" << strerror_r(errno, errtxt, sizeof(errtxt)) << std::endl;
        controller->SetFailed(errtxt);
        return;
    }
    close(m_clientfd);
}
bool KrpcChannel::newConnect(const char *ip, uint16_t port)
{
    // 使用socket网络编程
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd)
    {
        char errtxt[512] = {0};
        std::cout << "socket error" << strerror_r(errno, errtxt, sizeof(errtxt)) << std::endl;
        LOG(ERROR)<<"socket error:"<<errtxt;
        return false;
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);
    if (-1 == connect(clientfd, (struct sockaddr *)&server_addr, sizeof(server_addr)))
    {
        close(clientfd);
         char errtxt[512] = {0};
        std::cout << "connect error" << strerror_r(errno, errtxt, sizeof(errtxt)) << std::endl;
        LOG(ERROR)<< "connect server error"<<errtxt;
        return false;
    }
    m_clientfd = clientfd;
    return true;
}
std::string KrpcChannel::QueryServiceHost(ZkClient *zkclient,std::string service_name,std::string method_name,int& idx){
    std::string method_path = "/" + service_name + "/" + method_name;
    std::cout << "method_path: " << method_path << std::endl;
    std::unique_lock<std::mutex> lock(g_data_mutx);
    std::string host_data_1 = zkclient->GetData(method_path.c_str());
    lock.unlock(); 
    if (host_data_1 == "")
    {
        LOG(ERROR)<< method_path + " is not exist!";
        return" ";
    }
    idx = host_data_1.find(":"); // 127.0.0.1:8000 获取到ip和port的分割符
    if (idx == -1)
    {
        LOG(ERROR)<< method_path + " address is invalid!";
        return" ";
    }
    return host_data_1;
}
KrpcChannel::KrpcChannel(bool connectNow) : m_clientfd(-1),m_idx(0)
{
    if(!connectNow){
        return ;
    }
    auto rt=newConnect(m_ip.c_str(), m_port);//判断是否连接成功
    int count=3;//重试次数
    while(!rt&&count--){
        rt=newConnect(m_ip.c_str(), m_port);
    }
}