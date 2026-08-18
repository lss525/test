#include "../include/server/logger.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>


namespace chat {

void Logger::init(const std::string& log_file) {

    auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file, true);
    auto logger = std::make_shared<spdlog::logger>("chat", spdlog::sinks_init_list{console, file});
    
    logger->set_level(spdlog::level::info);

    spdlog::set_default_logger(logger);

}

} // namespace chat