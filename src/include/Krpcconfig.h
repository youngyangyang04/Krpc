#ifndef _Krpcconfig_h
#define _Krpcconfig_h
#include <unordered_map>
#include <string>
class Krpcconfig{
    public:
    void LoadConfigFile(const char *config_file);//加载配置文件
    std::string Load(const std::string &key);//查找key对应的value
    void WatchConfig();
    void ReloadConfig();
    bool IsConfigChanged();
    private:
    std::unordered_map<std::string, std::string> config_map;
    void Trim(std::string &read_buf);//去掉字符串前后的空格
    std::string m_config_file;
    time_t m_last_modified;
};
#endif
