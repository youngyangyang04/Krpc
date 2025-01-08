# Krpc

> ⭐️ 本项目为[【代码随想录知识星球】](https://programmercarl.com/other/kstar.html) 教学项目   
> ⭐️ 在 [手写RPC框架文档](https://www.programmercarl.com/other/project_C++RPC.html)  里详细讲解：**项目前置知识 + 项目细节 +  代码解读 + 项目难点 + 面试题与回答 + 简历写法  + 项目拓展**。 全面帮助你用这个项目求职面试！

# 项目概述

本项目基于protobuf的c++分布式网络通信框架，使用了zookeeper作为服务中间件，负责解决在分布式服务部署中 服务的发布与调用、消息的序列与反序列化、网络包的收发等问题，使其能提供高并发的远程函数调用服务，可以让使用者专注于业务，快速实现微服务的分布式部署，项目会继续完善的欢迎，大家一起学习。

## 运行环境

    Ubuntu 20.04 LTS

## 编译指令

    mkdir build && cd build && cmake .. && make
    进入到example目录下，运行./server和./client，即可完成服务发布和调用。

## 库准备

    muduo,https://blog.csdn.net/QIANGWEIYUAN/article/details/89023980
    zookeeper:
    安装 Zookeeper,sudo apt install libzookeeper-mt-dev
    安装 Zookeeper 开发库,sudo apt install libzookeeper-mt-dev
    protoc，本地版本为3.12.4，ubuntu22使用sudo apt-get install protobuf-compiler libprotobuf-dev安装默认就是这个版本
    glog安装：sudo apt-get install libgoogle-glog-dev libgflags-dev

## 整体的框架

muduo库：负责数据流的网络通信，采用了多线程epoll模式的IO多路复用，让服务发布端接受服务调用端的连接请求，并由绑定的回调函数处理调用端的函数调用请求

protobuf：负责rpc方法的注册，数据的序列化和反序列化，相比于文本存储的xml和json来说，protobuf是二进制存储，且不需要存储额外的信息，效率更高

zookeeper：负责分布式环境的服务注册，记录服务所在的ip地址以及port端口号，可动态地为调用端提供目标服务所在发布端的ip地址与端口号，方便服务所在ip地址变动的及时更新

TCP沾包问题处理：定义服务发布端和调用端之间的消息传输格式，记录方法名和参数长度，防止沾包。

后续增加了glog的日志库，进行异步的日志记录。

## 性能测试

在Kclient中进行了手写了一个简单的测试，可以作为一个性能参考，目前还不是最优还在继续优化。
