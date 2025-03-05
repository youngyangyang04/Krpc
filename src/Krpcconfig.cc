#include "Krpcconfig.h"
#include "memory"
#include <thread>
#include <chrono>
#include <iostream>
#include "KrpcLogger.h"
#include <sys/stat.h>

//加载配置文件
void Krpcconfig::LoadConfigFile(const char *config_file) {
    m_config_file = config_file;  // 保存配置文件路径
    struct stat file_stat;
    if (stat(config_file, &file_stat) == 0) {
        m_last_modified = file_stat.st_mtime;  // 保存文件最后修改时间
    }

    std::unique_ptr<FILE,decltype(&fclose)> pf(
        fopen(config_file, "r"),
        &fclose
    );
    if(pf==nullptr){
        exit(EXIT_FAILURE);   
    }
    char buf[1024];
    //使用pf.get()方法获取原始指针
    while(fgets(buf,1024,pf.get())!=nullptr){
        std::string read_buf(buf);
        Trim(read_buf);//去掉字符串前后的空格
        if(read_buf[0]=='#'||read_buf.empty())continue;
        int index=read_buf.find('=');
        if(index==-1)continue;
        std::string key=read_buf.substr(0,index);
        Trim(key);//去掉key前后的空格
        int endindex=read_buf.find('\n',index);//找到行尾
        std::string value=read_buf.substr(index+1,endindex-index-1);//找到value，-1目的是排除\n
        Trim(value);//去掉value前后的空格
        config_map.insert({key,value});
    }
}
//查找key对应的value
std::string Krpcconfig::Load(const std::string &key){
    auto it=config_map.find(key);
    if(it==config_map.end()){
        return "";
    }
    return it->second;
}
//去掉字符串前后的空格
void Krpcconfig::Trim(std::string &read_buf){
    int index=read_buf.find_first_not_of(' ');//去掉字符串前的空格
    if(index!=-1){
        read_buf=read_buf.substr(index,read_buf.size()-index);
    }
      index=read_buf.find_last_not_of(' ');//去掉字符串后的空格
    if(index!=-1){
        read_buf=read_buf.substr(0,index+1);
    }
}

void Krpcconfig::WatchConfig() {
    // 监听配置文件变化
    std::thread([this]() {
        while (true) {
            if (IsConfigChanged()) {
                LOG(INFO) << "检测到配置变更，重新加载";
                ReloadConfig();
            }
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }).detach();
}

bool Krpcconfig::IsConfigChanged() {
    struct stat file_stat;
    if (stat(m_config_file.c_str(), &file_stat) != 0) {
        return false;
    }
    
    if (file_stat.st_mtime > m_last_modified) {
        m_last_modified = file_stat.st_mtime;
        return true;
    }
    return false;
}

void Krpcconfig::ReloadConfig() {
    LoadConfigFile(m_config_file.c_str());
}