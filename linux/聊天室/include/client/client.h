
#ifndef CHAT_CLIENT_H
#define CHAT_CLIENT_H

#include <string>
#include <cstdint>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <thread>
#include <atomic>
#include "/home/lighning/codes/test/linux/聊天室/include/common/message.h"

namespace chat {


class Client {
public:
    Client(const std::string& host, int port);
    ~Client();

    bool connect();
    void disconnect();
    bool is_connected() const { return connected_; }

    // 发送原始数据
    void send_raw(const std::string& data);

    // 发送登录请求
    void send_login(const std::string& username, const std::string& password);
    
    // 发送注册请求
    void send_register(const std::string& username, const std::string& password,
                       const std::string& email, const std::string& phone);

    // 接收响应（阻塞等待）
    bool wait_response(MsgHeader& hdr, Buffer& body, int timeout_sec = 5);

    void send_verify_code(const std::string& target, uint8_t type);
    void send_login_with_code(const std::string& target, const std::string& code);
private:
    std::string host_;
    int port_;
    int sockfd_;
    std::atomic<bool> connected_{false};
    std::string recv_buffer_;
};

} // namespace chat

#endif