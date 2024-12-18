# Krpc
【代码随想录知识星球】项目分享-手撕RPC框架（CPP）
# 项目概述
本项目基于protobuf的c++分布式网络通信框架，使用了zookeeper作为服务中间件，负责解决在分布式服务部署中 服务的发布与调用、消息的序列与反序列化、网络包的收发等问题，使其能提供高并发的远程函数调用服务，可以让使用者专注于业务，快速实现微服务的分布式部署，项目会继续完善的欢迎，大家一起学习。
## 运行环境
    Ubuntu 20.04 LTS
## 编译指令
    mkdir build && cd build && cmake .. && make
    进入到example目录下，运行./server和./client，即可完成服务发布和调用。

## 库准备
1. Muduo 库的安装
Muduo 是一个基于多线程 Epoll 模式的高效网络库，负责数据流的网络通信。
安装教程参考：[Mudo安装](https://blog.csdn.net/QIANGWEIYUAN/article/details/89023980)

2. Zookeeper 的安装
Zookeeper 负责服务注册与发现，动态记录服务的 IP 地址及端口号，以便调用端快速找到目标服务。
安装步骤：
安装 Zookeeper：
sudo apt install libzookeeper-mt-dev
安装 Zookeeper 开发库：
sudo apt install libzookeeper-mt-dev

3. Protobuf 的安装
Protobuf 负责 RPC 方法的注册、数据的序列化与反序列化。
相较于 XML 和 JSON，Protobuf 是二进制存储，效率更高。
本地版本：3.12.4
在 Ubuntu 22 上可以直接安装：
sudo apt-get install protobuf-compiler libprotobuf-dev

4. Glog 日志库的安装
Glog 是一个高效的异步日志库，用于记录框架运行时的调试与错误日志。
sudo apt-get install libgoogle-glog-dev libgflags-dev
## 整体的框架
muduo库：负责数据流的网络通信，采用了多线程epoll模式的IO多路复用，让服务发布端接受服务调用端的连接请求，并由绑定的回调函数处理调用端的函数调用请求

protobuf：负责rpc方法的注册，数据的序列化和反序列化，相比于文本存储的xml和json来说，protobuf是二进制存储，且不需要存储额外的信息，效率更高

zookeeper：负责分布式环境的服务注册，记录服务所在的ip地址以及port端口号，可动态地为调用端提供目标服务所在发布端的ip地址与端口号，方便服务所在ip地址变动的及时更新

TCP沾包问题处理：定义服务发布端和调用端之间的消息传输格式，记录方法名和参数长度，防止沾包。

后续增加了glog的日志库，进行异步的日志记录。

## 性能测试
在Kclient中进行了手写了一个简单的测试，可以作为一个性能参考，目前还不是最优还在继续优化。