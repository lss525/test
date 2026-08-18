#include "../include/common/config.h"
#include <fstream>

namespace chat {

std::map<std::string, std::string> Config::kv_;

namespace {
std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}
} // namespace

bool Config::load(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        if (!key.empty()) kv_[key] = val;
    }
    return true;
}

std::string Config::get(const std::string& key, const std::string& def) {
    auto it = kv_.find(key);
    return it == kv_.end() ? def : it->second;
}

int Config::get_int(const std::string& key, int def) {
    auto it = kv_.find(key);
    if (it == kv_.end()) return def;
    try {
        return std::stoi(it->second);
    } catch (...) {
        return def;
    }
}

} // namespace chat
