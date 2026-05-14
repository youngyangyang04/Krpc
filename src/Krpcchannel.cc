#include "Krpcchannel.h"
#include "Krpcheader.pb.h"
#include "Krpcapplication.h"
#include "Krpccontroller.h"
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "KrpcLogger.h"
#include "KrpcConnectPool.h"

static std::atomic<uint64_t> g_req_id{0};

ssize_t KrpcChannel::recv_exact(int fd, char *buf, size_t size) {
    size_t total_read = 0;
    while (total_read < size) {
        ssize_t ret = recv(fd, buf + total_read, size - total_read, 0);
        if (ret == 0) return 0; 
        if (ret == -1) {
            if (errno == EINTR) continue; 
            return -1; 
        }
        total_read += ret;
    }
    return total_read;
}

void KrpcChannel::CallMethod(const ::google::protobuf::MethodDescriptor *method,
                             ::google::protobuf::RpcController *controller,
                             const ::google::protobuf::Message *request,
                             ::google::protobuf::Message *response,
                             ::google::protobuf::Closure *done)
{
    std::string service_name = method->service()->name();    
    std::string method_name = method->name(); 

    static std::once_flag init_flag;
    std::call_once(init_flag, []() { ServiceDiscovery::GetInstance().Init(); });

   
    uint64_t req_id = g_req_id.fetch_add(1, std::memory_order_relaxed);
    std::string route_key = std::to_string(req_id);

    std::string ip_port = ServiceDiscovery::GetInstance().GetTargetNode(service_name, route_key);
    if (ip_port.empty()) {
        controller->SetFailed("Hash ring returned empty node!");
        return;
    }
    
    size_t pos = ip_port.find(":");
    std::string target_ip = ip_port.substr(0, pos);
    uint16_t target_port = atoi(ip_port.substr(pos + 1).c_str());

  
    int clientfd = KrpcConnectPool::GetInstance().BorrowConnection(target_ip, target_port);
    if (clientfd == -1) {
        controller->SetFailed("Borrow connection from pool failed!");
        return;
    }

    bool is_bad = false; 
    std::string args_str;
    
    if (!request->SerializeToString(&args_str)) {
        controller->SetFailed("Serialize request fail");
        is_bad = true; 
    } else {
        Krpc::RpcHeader krpcheader;
        krpcheader.set_service_name(service_name);
        krpcheader.set_method_name(method_name);
        krpcheader.set_args_size(args_str.size());

        std::string rpc_header_str;
        krpcheader.SerializeToString(&rpc_header_str);

        uint32_t header_size = rpc_header_str.size();
        uint32_t total_len = 4 + header_size + args_str.size();
        uint32_t net_total_len = htonl(total_len);
        uint32_t net_header_len = htonl(header_size);

        std::string send_rpc_str;
        send_rpc_str.reserve(4 + 4 + header_size + args_str.size());
        send_rpc_str.append((char *)&net_total_len, 4);
        send_rpc_str.append((char *)&net_header_len, 4);
        send_rpc_str.append(rpc_header_str);
        send_rpc_str.append(args_str);

       
        if (send(clientfd, send_rpc_str.c_str(), send_rpc_str.size(), MSG_NOSIGNAL) == -1) {
            controller->SetFailed("Send rpc request error!");
            is_bad = true;
        } else {
            uint32_t response_len = 0;
            if (recv_exact(clientfd, (char *)&response_len, 4) != 4) {
                controller->SetFailed("Recv response header error!");
                is_bad = true;
            } else {
                response_len = ntohl(response_len);
                std::vector<char> recv_buf(response_len);
                if (recv_exact(clientfd, recv_buf.data(), response_len) != (ssize_t)response_len) {
                    controller->SetFailed("Recv response body error!");
                    is_bad = true;
                } else {
                    if (!response->ParseFromArray(recv_buf.data(), response_len)) {
                        controller->SetFailed("Parse response error!");
                        is_bad = true;
                    }
                }
            }
        }
    }

 
    KrpcConnectPool::GetInstance().ReturnConnection(target_ip, target_port, clientfd, is_bad);
}