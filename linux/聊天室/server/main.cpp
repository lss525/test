#include "../include/server/server.h"
#include "../include/server/logger.h"
#include "../include/common/config.h"
#include <iostream>
#include <csignal>


chat::Server* g_server = nullptr;

void signal_handler(int sig) {
    if (g_server) {
        LOG_INFO("Signal {} received, shutting down", sig);
        g_server->stop();
    }
}

int main(int argc, char* argv[]) {

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <port> [bind_address]" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);
    std::string address = argc > 2 ? argv[2] : "0.0.0.0";

    chat::Logger::init("chat_server.log");
    LOG_INFO("Server starting on {}:{}", address, port);

    // 加载配置文件（密码/授权码从 config.ini 读取，不写死在代码里）
    std::string config_path = argc > 3 ? argv[3] : "";
    if (!config_path.empty()) {
        chat::Config::load(config_path);
    } else if (!chat::Config::load("config.ini") && !chat::Config::load("../config.ini")) {
        LOG_WARN("未找到 config.ini，数据库/邮箱将使用默认或空配置");
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    chat::Server server(port, address);
    g_server = &server;

    if (!server.initialize()) {
        LOG_ERROR("Server initialization failed");
        return 1;
    }

    server.run();
    LOG_INFO("Server stopped");
    return 0;
}