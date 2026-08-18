#ifndef CHAT_CONFIG_H
#define CHAT_CONFIG_H

#include <string>
#include <map>

namespace chat {

// 极简 INI 风格配置加载器，格式：key = value，# 开头为注释
class Config {
public:
    static bool load(const std::string& path);

    static std::string get(const std::string& key, const std::string& def = "");
    static int get_int(const std::string& key, int def = 0);

private:
    static std::map<std::string, std::string> kv_;
};

} // namespace chat

#endif
