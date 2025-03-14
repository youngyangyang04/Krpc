#include "Krpcconfig.h"
#include "memory"

// 加载配置文件，解析配置文件中的键值对
void Krpcconfig::LoadConfigFile(const char *config_file) {
    // 使用智能指针管理文件指针，确保文件在退出时自动关闭
    std::unique_ptr<FILE, decltype(&fclose)> pf(
        fopen(config_file, "r"),  // 打开配置文件
        &fclose  // 文件关闭函数
    );

    if (pf == nullptr) {  // 如果文件打开失败
        exit(EXIT_FAILURE);  // 退出程序
    }

    char buf[1024];  // 用于存储从文件中读取的每一行内容
    // 使用pf.get()方法获取原始指针，逐行读取文件内容
    while (fgets(buf, 1024, pf.get()) != nullptr) {
        std::string read_buf(buf);  // 将读取的内容转换为字符串
        Trim(read_buf);  // 去掉字符串前后的空格

        // 忽略注释行（以#开头）和空行
        if (read_buf[0] == '#' || read_buf.empty()) continue;

        // 查找键值对的分隔符'='
        int index = read_buf.find('=');
        if (index == -1) continue;  // 如果没有找到'='，跳过该行

        // 提取键（key）
        std::string key = read_buf.substr(0, index);
        Trim(key);  // 去掉key前后的空格

        // 查找行尾的换行符
        int endindex = read_buf.find('\n', index);
        // 提取值（value），并去掉换行符
        std::string value = read_buf.substr(index + 1, endindex - index - 1);
        Trim(value);  // 去掉value前后的空格

        // 将键值对存入配置map中
        config_map.insert({key, value});
    }
}

// 根据key查找对应的value
std::string Krpcconfig::Load(const std::string &key) {
    auto it = config_map.find(key);  // 在map中查找key
    if (it == config_map.end()) {  // 如果未找到
        return "";  // 返回空字符串
    }
    return it->second;  // 返回对应的value
}

// 去掉字符串前后的空格
void Krpcconfig::Trim(std::string &read_buf) {
    // 去掉字符串前面的空格
    int index = read_buf.find_first_not_of(' ');
    if (index != -1) {  // 如果找到非空格字符
        read_buf = read_buf.substr(index, read_buf.size() - index);  // 截取字符串
    }

    // 去掉字符串后面的空格
    index = read_buf.find_last_not_of(' ');
    if (index != -1) {  // 如果找到非空格字符
        read_buf = read_buf.substr(0, index + 1);  // 截取字符串
    }
}