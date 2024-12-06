#include "zookeeperutil.h"
#include"Krpcapplication.h"
//全局的watcher观察器，zkserver给zkclient的通知
void global_watcher(zhandle_t *zh ,int type,int status,const char* path,void* watcherCtx)
{
    if(type==ZOO_SESSION_EVENT)//回调消息类型和会话相关的消息类型
    {
        if(status==ZOO_CONNECTED_STATE)//zkclient和zkserver连接成功
        {
            sem_t* mysem=(sem_t*)zoo_get_context(zh);
            sem_post(mysem);//通知阻塞在sem_wait上的线程，sem_wait函数会解除阻塞
        }
    }
}
 ZkClient::ZkClient():m_zhandle(nullptr){

 }
ZkClient::~ZkClient(){
if(m_zhandle!=nullptr)
{
    zookeeper_close(m_zhandle);
}
}
void ZkClient::Start(){
    
    std::string host=KrpcApplication::GetInstance().GetConfig().Load("zookeeperip");//获取zookeeper服务端的ip
    std::string port=KrpcApplication::GetInstance().GetConfig().Load("zookeeperport");//获取zoo keeper服务端的port
    std::string connstr=host+":"+port;
	/*
	zookeeper_mt：多线程版本
	zookeeper的API客户端程序提供了三个线程
	API调用线程 
	网络I/O线程  pthread_create  poll
	watcher回调线程 pthread_create
	*/
    //使用zookeeper_init初始化一个zk对象，异步建立rpcserver和zkclient之间的连接
    m_zhandle=zookeeper_init(connstr.c_str(),global_watcher,30000,nullptr,nullptr,0);
    if(nullptr==m_zhandle){//这个返回值不代表连接成功或者不成功
        std::cout<<"zookeeper_init error"<<std::endl;
        exit(EXIT_FAILURE);
    }
    //使用信号量的目的，因为zookeeper是异步的，我们不知道创建节点是否成功
    sem_t sem;//设置信号量
    sem_init(&sem,0,0);//初始化信号量
    zoo_set_context(m_zhandle,&sem);//给这个句柄添加一些额外的信息。
    sem_wait(&sem);//阻塞结束后
    std::cout<<"zookeeper_init success"<<std::endl;

}
void ZkClient::Create(const char* path,const char* data,int datalen,int state){
    //创建znode节点，可以选择永久性节点还是临时节点。
    char path_buffer[128];
    int bufferlen=sizeof(path_buffer);
    int flag=zoo_exists(m_zhandle,path,0,nullptr);
    if(flag==ZNONODE)//表示节点不存在
    {
        //创建指定的path的znode节点
        flag=zoo_create(m_zhandle,path,data,datalen,&ZOO_OPEN_ACL_UNSAFE,state,path_buffer,bufferlen);
        if(flag==ZOK)
        {
            std::cout<<"znode create success... path:"<<path<<std::endl;
        }else{
            std::cout<<"flag"<<flag<<std::endl;
            std::cout<<"znode create error... path:"<<path<<std::endl;
            exit(EXIT_FAILURE);
        }
    }

}
std::string ZkClient::GetData(const char* path){
    char buf[64];
    int bufferlen=sizeof(buf);
    int flag=zoo_get(m_zhandle,path,0,buf,&bufferlen,nullptr);
    if(flag!=ZOK){
        std::cout<<"zoo_get error"<<std::endl;
            return "";
    }else{
        return buf;
    }
     return "";
}
