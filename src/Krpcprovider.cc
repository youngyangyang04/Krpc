#include "Krpcprovider.h"
#include "Krpcapplication.h"
#include "Krpcheader.pb.h"
#include "KrpcLogger.h"
#include <iostream>

class KrpcClosure : public google::protobuf::Closure
{
public:
    explicit KrpcClosure(std::function<void()> cb) : cb_(std::move(cb)) {}
    void Run() override
    {
        cb_();
        delete this;
    }
private:
    std::function<void()> cb_;
};

void KrpcProvider::NotifyService(google::protobuf::Service *service)
{
    ServiceInfo service_info;
    const google::protobuf::ServiceDescriptor *psd = service->GetDescriptor();
    std::string service_name = psd->name();
    int method_count = psd->method_count();

    std::cout << "service_name=" << service_name << std::endl;

    for (int i = 0; i < method_count; ++i)
    {
        const google::protobuf::MethodDescriptor *pmd = psd->method(i);
        std::string method_name = pmd->name();
        std::cout << "method_name=" << method_name << std::endl;
        service_info.method_map.emplace(method_name, pmd); 
    }
    service_info.service = service;                  
    service_map.emplace(service_name, service_info); 
}

void KrpcProvider::Run()
{
    std::string ip = KrpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    int port = atoi(KrpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());

    muduo::net::InetAddress address(ip, port);
    std::shared_ptr<muduo::net::TcpServer> server = std::make_shared<muduo::net::TcpServer>(&event_loop, address, "KrpcProvider");

    server->setConnectionCallback(std::bind(&KrpcProvider::OnConnection, this, std::placeholders::_1));
    server->setMessageCallback(std::bind(&KrpcProvider::OnMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    // IO线程数量 (负责收发字节流)
    server->setThreadNum(4);

    m_thread_pool.start(100);

    ZkClient zkclient;
    zkclient.Start(); 
                      
    for (auto &sp : service_map)
    {
        std::string service_path = "/" + sp.first;
        zkclient.Create(service_path.c_str(), nullptr, 0);

        std::string ip_port = ip + ":" + std::to_string(port);
        std::string instance_path = service_path + "/" + ip_port;
        zkclient.Create(instance_path.c_str(), ip_port.c_str(), ip_port.length(), ZOO_EPHEMERAL);
    }

    std::cout << "RpcProvider start service at ip:" << ip << " port:" << port << std::endl;

    server->start();
    event_loop.loop(); 
}

void KrpcProvider::OnConnection(const muduo::net::TcpConnectionPtr &conn)
{
    if (!conn->connected())
    {
        conn->shutdown();
    }
}

void KrpcProvider::OnMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buffer, muduo::Timestamp receive_time)
{
    while (buffer->readableBytes() >= 4)
    {
        uint32_t total_len = 0;
        std::memcpy(&total_len, buffer->peek(), 4);
        total_len = ntohl(total_len); 

        if (buffer->readableBytes() < 4 + total_len)
        {
            break;
        }

        buffer->retrieve(4); 

        uint32_t header_len = 0;
        const char *data_ptr = buffer->peek();
        std::memcpy(&header_len, data_ptr, 4);
        header_len = ntohl(header_len);
        buffer->retrieve(4); 

        std::string rpc_header_str(buffer->peek(), header_len);
        Krpc::RpcHeader krpcHeader;
        buffer->retrieve(header_len); 

        uint32_t args_size = total_len - 4 - header_len; 
        std::string args_str(buffer->peek(), args_size);
        buffer->retrieve(args_size); 

        if (!krpcHeader.ParseFromString(rpc_header_str))
        {
            std::cout << "header parse error" << std::endl;
            return;
        }

        std::string service_name = krpcHeader.service_name();
        std::string method_name = krpcHeader.method_name();

        auto it = service_map.find(service_name);
        if (it == service_map.end())
        {
            std::cout << service_name << " is not exist!" << std::endl;
            return;
        }
        auto mit = it->second.method_map.find(method_name);
        if (mit == it->second.method_map.end())
        {
            std::cout << service_name << "." << method_name << " is not exist!" << std::endl;
            return;
        }

        google::protobuf::Service *service = it->second.service;
        const google::protobuf::MethodDescriptor *method = mit->second;

        google::protobuf::Message *request = service->GetRequestPrototype(method).New();
        if (!request->ParseFromString(args_str))
        {
            delete request;
            return;
        }
        google::protobuf::Message *response = service->GetResponsePrototype(method).New();

        google::protobuf::Closure *done = new KrpcClosure(
            [this, conn, response, request]()
            {
                this->SendRpcResponse(conn, response, request);
            });
            
        
        m_thread_pool.run([service, method, request, response, done]() {
            service->CallMethod(method, nullptr, request, response, done);
        });
        
        // //空跑框架测试
        // service->CallMethod(method, nullptr, request, response, done);
    }
}

void KrpcProvider::SendRpcResponse(const muduo::net::TcpConnectionPtr &conn, google::protobuf::Message *response, google::protobuf::Message *request)
{
    std::string response_str;
    if (response->SerializeToString(&response_str))
    {
        uint32_t len = response_str.size();
        uint32_t net_len = htonl(len);

        std::string send_buf;
        send_buf.resize(4 + len);
        std::memcpy(&send_buf[0], &net_len, 4);
        std::memcpy(&send_buf[4], response_str.data(), len);

        // muduo 的 conn->send 是线程安全的，它会自动将发送任务转回原来的 I/O 线程执行
        conn->send(send_buf);
    }
    else
    {
        LOG(ERROR) << "serialize response error!";
    }

    delete response;
    delete request;
}

KrpcProvider::~KrpcProvider()
{
    std::cout << "~KrpcProvider()" << std::endl;
    for (auto &sp : service_map)
    {
        if (sp.second.service != nullptr)
        {
            delete sp.second.service;
            sp.second.service = nullptr;
        }
    }
    event_loop.quit(); 
}