#ifndef CHAT_LOGGER_H
#define CHAT_LOGGER_H

#include <string>
#include <spdlog/spdlog.h>

namespace chat {

class Logger {
public:
    static void init(const std::string& log_file);
};

} // namespace chat

#define LOG_INFO(...)  spdlog::info(__VA_ARGS__)
#define LOG_WARN(...)  spdlog::warn(__VA_ARGS__)
#define LOG_ERROR(...) spdlog::error(__VA_ARGS__)

#endif