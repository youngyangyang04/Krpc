#include "Krpcapplication.h"
#include "../user.pb.h"
#include"Krpccontroller.h"
#include <iostream>

int main(int argc,char** argv)
{
    //整个程序启动以后，想使用krpc框架就要先调用初始化函数(只初始化一次)
    KrpcApplication::Init(argc,argv);

    //演示用远程发布rpc方法Login
    Kuser::UserServiceRpc_Stub stub(new KrpcChannel());

    //rpc方法请求参数
    Kuser::LoginRequest request;
    request.set_name("zhangsan");
    request.set_pwd("123456");

    //rpc方法响应参数                                                    
    Kuser::LoginResponse response;        
    Krpccontroller controller;             //我们重写的Rpchannel继承google的rpc框架的rpcchannel,形成多态
    stub.Login(&controller,&request,&response,nullptr);//父类Rpchannel->子类Rpchannle::callmethod 
 // 一次rpc调用完成，读调用的结果
    if (controller.Failed())
    {
        std::cout<< controller.ErrorText() <<std::endl;
    }
    else
    {
        if (0 == response.result().errcode())
        {
            std::cout << "rpc login response success:" << response.success() << std::endl;
        }
        else
        {
            std::cout << "rpc login response error : " << response.result().errmsg() << std::endl;
        }
    }

    return 0;
}
