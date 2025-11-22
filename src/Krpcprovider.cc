#include "Krpcprovider.h"
#include "Krpcapplication.h"
#include "Krpcheader.pb.h"
#include "KrpcLogger.h"
#include <iostream>



// 注册服务对象及其方法，以便服务端能够处理客户端的RPC请求
void KrpcProvider::NotifyService(google::protobuf::Service *service) {
    // 服务端需要知道客户端想要调用的服务对象和方法，
    // 这些信息会保存在一个数据结构（如 ServiceInfo）中。
    ServiceInfo service_info;

    // 参数类型设置为 google::protobuf::Service，是因为所有由 protobuf 生成的服务类
    // 都继承自 google::protobuf::Service，这样我们可以通过基类指针指向子类对象，
    // 实现动态多态。

    // 通过动态多态调用 service->GetDescriptor()，
    // GetDescriptor() 方法会返回 protobuf 生成的服务类的描述信息（ServiceDescriptor）。
    const google::protobuf::ServiceDescriptor *psd = service->GetDescriptor();

    // 通过 ServiceDescriptor，我们可以获取该服务类中定义的方法列表，
    // 并进行相应的注册和管理。

    // 获取服务的名字
    std::string service_name = psd->name();
    // 获取服务端对象service的方法数量
    int method_count = psd->method_count();

    // 打印服务名
    std::cout << "service_name=" << service_name << std::endl;

    // 遍历服务中的所有方法，并注册到服务信息中
    for (int i = 0; i < method_count; ++i) {
        // 获取服务中的方法描述
        const google::protobuf::MethodDescriptor *pmd = psd->method(i);
        std::string method_name = pmd->name();
        std::cout << "method_name=" << method_name << std::endl;
        service_info.method_map.emplace(method_name, pmd);  // 将方法名和方法描述符存入map
    }
    service_info.service = service;  // 保存服务对象
    service_map.emplace(service_name, service_info);  // 将服务信息存入服务map
}

// 启动RPC服务节点，开始提供远程网络调用服务
void KrpcProvider::Run() {
    // 读取配置文件中的RPC服务器IP和端口
    std::string ip = KrpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    int port = atoi(KrpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());

    // 使用muduo网络库，创建地址对象
    muduo::net::InetAddress address(ip, port);

    // 创建TcpServer对象
    std::shared_ptr<muduo::net::TcpServer> server = std::make_shared<muduo::net::TcpServer>(&event_loop, address, "KrpcProvider");

    // 绑定连接回调和消息回调，分离网络连接业务和消息处理业务
    server->setConnectionCallback(std::bind(&KrpcProvider::OnConnection, this, std::placeholders::_1));
    server->setMessageCallback(std::bind(&KrpcProvider::OnMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    // 设置muduo库的线程数量
    server->setThreadNum(4);

    // 将当前RPC节点上要发布的服务全部注册到ZooKeeper上，让RPC客户端可以在ZooKeeper上发现服务
    ZkClient zkclient;
    zkclient.Start();  // 连接ZooKeeper服务器
    // service_name为永久节点，method_name为临时节点
    for (auto &sp : service_map) {
        // service_name 在ZooKeeper中的目录是"/"+service_name
        std::string service_path = "/" + sp.first;
        zkclient.Create(service_path.c_str(), nullptr, 0);  // 创建服务节点
        for (auto &mp : sp.second.method_map) {
            std::string method_path = service_path + "/" + mp.first;
            char method_path_data[128] = {0};
            sprintf(method_path_data, "%s:%d", ip.c_str(), port);  // 将IP和端口信息存入节点数据
            // ZOO_EPHEMERAL表示这个节点是临时节点，在客户端断开连接后，ZooKeeper会自动删除这个节点
            zkclient.Create(method_path.c_str(), method_path_data, strlen(method_path_data), ZOO_EPHEMERAL);
        }
    }

    // RPC服务端准备启动，打印信息
    std::cout << "RpcProvider start service at ip:" << ip << " port:" << port << std::endl;

    // 启动网络服务
    server->start();
    event_loop.loop();  // 进入事件循环
}

// 连接回调函数，处理客户端连接事件
void KrpcProvider::OnConnection(const muduo::net::TcpConnectionPtr &conn) {
    if (!conn->connected()) {
        // 如果连接关闭，则断开连接
        conn->shutdown();
    }
}

// 消息回调函数，处理客户端发送的RPC请求
void KrpcProvider::OnMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buffer, muduo::Timestamp receive_time) {
    std::cout << "OnMessage" << std::endl;

     // 循环处理缓冲区，解决粘包问题
    while (buffer->readableBytes() >= 4) {
        // 1. 预读取前4个字节（Total Length）
        // peek() 不会移动 buffer 的读指针
        uint32_t total_len = 0;
        std::memcpy(&total_len, buffer->peek(), 4);
        total_len = ntohl(total_len); // 网络字节序转主机字节序

        // 2. 检查数据是否完整（拆包处理）
        // 如果缓冲区剩余数据 小于 4字节长度头 + 内容长度，说明包没收全，退出等待下一次
        if (buffer->readableBytes() < 4 + total_len) {
            break; 
        }

        // --- 数据包完整，开始解包 ---
        
        // 3. 真正读取数据
        buffer->retrieve(4); // 消耗掉前4个字节的长度头
        
        // 读取 4字节的 Header Length
        uint32_t header_len = 0;
        const char* data_ptr = buffer->peek();
        std::memcpy(&header_len, data_ptr, 4);
        header_len = ntohl(header_len);
        
        buffer->retrieve(4); // 消耗掉 header length

        // 读取 Header 数据
        std::string rpc_header_str(buffer->peek(), header_len);
        Krpc::RpcHeader krpcHeader;
        buffer->retrieve(header_len); // 消耗掉 header data

        // 读取 Body 数据 (args)
        uint32_t args_size = total_len - 4 - header_len; // 总长度 - header长度字段(4) - header内容
        std::string args_str(buffer->peek(), args_size);
        buffer->retrieve(args_size); // 消耗掉 body data

        // 4. 业务逻辑处理
        if (!krpcHeader.ParseFromString(rpc_header_str)) {
            std::cout << "header parse error" << std::endl;
            return;
        }
        
        std::string service_name = krpcHeader.service_name();
        std::string method_name = krpcHeader.method_name();

        auto it = service_map.find(service_name);
        if (it == service_map.end()) {
            std::cout << service_name << " is not exist!" << std::endl;
            return;
        }
        auto mit = it->second.method_map.find(method_name);
        if (mit == it->second.method_map.end()) {
            std::cout << service_name << "." << method_name << " is not exist!" << std::endl;
            return;
        }

        google::protobuf::Service *service = it->second.service;
        const google::protobuf::MethodDescriptor *method = mit->second;

        google::protobuf::Message *request = service->GetRequestPrototype(method).New();
        if (!request->ParseFromString(args_str)) {
            std::cout << "request parse error" << std::endl;
            return;
        }
        google::protobuf::Message *response = service->GetResponsePrototype(method).New();

        google::protobuf::Closure *done = google::protobuf::NewCallback<KrpcProvider,
                                                                        const muduo::net::TcpConnectionPtr &,
                                                                        google::protobuf::Message *>(this,
                                                                                                     &KrpcProvider::SendRpcResponse,
                                                                                                     conn, response);
        service->CallMethod(method, nullptr, request, response, done);
    }
}

// 发送RPC响应给客户端
void KrpcProvider::SendRpcResponse(const muduo::net::TcpConnectionPtr &conn, google::protobuf::Message *response) {
    std::string response_str;
    if (response->SerializeToString(&response_str)) {
        // 构造响应：[4 bytes Total Len] + [Response Data]
        uint32_t len = response_str.size();
        uint32_t net_len = htonl(len);

        std::string send_buf;
        send_buf.resize(4 + len);
        
        // 填入长度
        std::memcpy(&send_buf[0], &net_len, 4);
        // 填入数据
        std::memcpy(&send_buf[4], response_str.data(), len);

        conn->send(send_buf);
    } else {
        std::cout << "serialize response error!" << std::endl;
    }
}

// 析构函数，退出事件循环
KrpcProvider::~KrpcProvider() {
    std::cout << "~KrpcProvider()" << std::endl;
    event_loop.quit();  // 退出事件循环
}