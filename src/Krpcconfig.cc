#include "Krpcconfig.h"
#include "memory"
//加载配置文件
void Krpcconfig::LoadConfigFile(const char *config_file){
    std::unique_ptr<FILE,decltype(&fclose)>pf(
        fopen(config_file,"r"),
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