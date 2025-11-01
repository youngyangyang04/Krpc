# Krpc

本项目如果是有C++语法基础的录友，且做过[知识星球](https://programmercarl.com/other/kstar.html)的：[基于Raft共识算法的KV数据库](https://programmercarl.com/other/project_fenbushi.html)，[协程库](https://programmercarl.com/other/project_coroutine.html)，那大家上手这个项目的时间会非常快：

学习时间：一天只需要抽3-4个小时，看3天左右基本能看完整个项目。

如果你还是新手，很多理论知识还要从头学习，如果一天学6-8小时，大概需要两周基本能完成这个RPC项目。

## 做完本项目你讲收获

* 深入理解RPC框架原理与分布式系统设计
* 夯实C++面向对象、STL、设计模式核心功底
* 掌握Socket、TCP/UDP及高并发I/O模型（epoll）
* 基于Muduo库实现Reactor网络模型，解耦业务与通信
* 熟练使用Protobuf定义消息、实现高效序列化/反序列化
* 设计自定义协议，解决TCP粘包/拆包问题
* 集成Zookeeper作为注册中心，实现服务注册与发现
* 运用Watcher机制动态感知服务状态，保障高可用
* 从0到1打造高性能RPC框架，获得分布式系统开发经验
* 提升解决复杂工程问题（协议设计、高并发、解耦）的能力

## 为什么要做c++版的rpc？

1.高性能需求

c++以其高效的内存管理和底层控制能力，成为性能要求比较高的系统(如金融、游戏服务器、实时通信系统)的首选语言。

在这些场景下，RPC框架需要尽可能减少通信开销，而C++天生的性能优势可以满足这一需要求。


2.系统级开发

很多底层基础设施(如数据库、中间件、分布式存储系统)都是用c++开发。

这些系统需要一个与语言无缝结合的高效RPC框架，避免因语言间的切换导致性能损耗


3.跨平台

C++的可移植性在不同平台(如linux、Windows、嵌入式系统)上广泛使用。

一个C++RPC框架能够为这些多平台环境提供统一的通信接口，降低开发成本。

4.灵活性与可扩展性

与某些语言的封闭生态不同，C++允许开发者灵活地调整底层实现。例如：可以定制序列化协议(如Protobuf、Thrift)、网络传输方式(如TCP、UDP、QUIC)等，以满足不同场景的需求。

关于C++版RPC框架的使用场景：

* 微服务架构：在微服务架构中，服务通常分布在不同的网络和不同的服务器上，此时就需要一个高效的通信手段就是我们的rpc。
* 实时通信：如在线游戏、视频直播、即时通信等场景，要求低延迟和高吞吐。C++RPC可以通过优化网络传输协议和序列化协议，提供实时性保障。
* 分布式存储与计算：像hadoop、或者你做个raft的共识算法的话，也可也发现我们在不同节点之间使用rpc传递数据进行通信。
* 嵌入式系统： 在嵌入式设备之间的通信中，资源有限且性能要求严格。C++的轻量级特性使其成为嵌入式RPC实现的理想选择。
* 跨语言调用： C++ RPC框架通常支持多语言绑定（如Python、Java），可以用作跨语言调用的桥梁。例如，在后端服务使用C++开发的情况下，前端服务可以通过RPC框架调用其功能。

## 项目专栏

在项目专栏中， 该**项目简历如何写、性能如何测试、项目怎么优化、面试都会问哪些问题**，都安排好了。

不仅如此，还有 「技术栈需求」「运行环境」「RPC理论」「日志库」「代码解读」

### 简历写法

专栏里直接给出简历写法， 项目难点 和 个人收获是面试官最关心的部分。

<div align="center"><img src='https://file1.kamacoder.com/i/algo/20250103223303.png' width=500 alt=''></img></div>

在[知识星球](https://programmercarl.com/other/kstar.html)RPC项目专栏 会给出本项目的参考简历写法，为了不让 这些写法重复率太高，所以公众号上是打码的。

### 性能测试

带大家测试RPC的性能，更充分了解 系统的表现。

<div align="center"><img src='https://file1.kamacoder.com/i/algo/20250103224011.png' width=500 alt=''></img></div>

### 项目优化

项目文档列出的十几个优化点：

<div align="center"><img src='https://file1.kamacoder.com/i/algo/20250103224306.png' width=500 alt=''></img></div>

涉及到 「通信模块」 「服务注册与发现模块」「负载均衡模块」「零拷贝优化技术」「日志与监控模块」「健康检测与熔断机制」「重试与超时处理」

从各个方面，带大家去了解项目如何进一步优化，帮助大家找到可以拓展的方向，打造自己的项目竞争力，也避免了项目重复。

### 代码讲解

给出项目整体流程图：

<div align="center"><img src='https://file1.kamacoder.com/i/algo/20250103224628.png' width=500 alt=''></img></div>

其中项目的所有代码以及每个函数和类都有详细解释，根本不用担心自己看不懂：

<div align="center"><img src='https://file1.kamacoder.com/i/algo/20250103224733.png' width=500 alt=''></img></div>

同时我们对项目中需要用到的日志库做了详细的分析：

<div align="center"><img src='https://file1.kamacoder.com/i/algo/20250103225024.png' width=500 alt=''></img></div>

### RPC理论

项目文档帮大家梳理清楚 RPC 的来龙去脉 ：

<div align="center"><img src='https://file1.kamacoder.com/i/algo/20250103224858.png' width=500 alt=''></img></div>

### 突击来用

如果大家面试在即，实在没时间做项目了，可以直接按照专栏给出的简历写法，写到简历上，然后把项目专栏里的面试问题，都认真背一背就好了，基本覆盖 绝大多数 RPC项目问题。


## 获取本项目专栏

本文档仅为星球内部专享，大家可以加入[知识星球](https://programmercarl.com/other/kstar.html)里获取。

