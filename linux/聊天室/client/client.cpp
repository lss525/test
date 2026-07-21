#include "/home/lighning/codes/test/linux/聊天室/include/client/client.h"

namespace chat {

Client::Client(const std::string& host, int port)
    : host_(host), port_(port), sockfd_(-1) {}

Client::~Client() {
    disconnect();
}

bool Client::connect() {
    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_ < 0) {
        std::cerr << "创建 socket 失败" << std::endl;
        return false;
    }

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    inet_pton(AF_INET, host_.c_str(), &addr.sin_addr);

    if (::connect(sockfd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "连接服务器失败: " << host_ << ":" << port_ << std::endl;
        close(sockfd_);
        sockfd_ = -1;
        return false;
    }

    connected_ = true;
    std::cout << "已连接到服务器 " << host_ << ":" << port_ << std::endl;
    return true;
}

void Client::disconnect() {
    if (sockfd_ >= 0) {
        close(sockfd_);
        sockfd_ = -1;
    }
    connected_ = false;
}

void Client::send_raw(const std::string& data) {
    if (!connected_) return;
    ::send(sockfd_, data.c_str(), data.size(), 0);
}

void Client::send_login(const std::string& username, const std::string& password) {
    // 打包消息体
    Buffer body;
    body.write_string(username);
    body.write_string(password);

    // 打包消息头 + 体
    std::string packet = MessageHelper::pack_header(
        MsgType::LOGIN_REQUEST, body.data().size(), 1);
    packet.append(body.data());

    send_raw(packet);
}

void Client::send_register(const std::string& username, const std::string& password,
                            const std::string& email, const std::string& phone) {
    Buffer body;
    body.write_string(username);
    body.write_string(password);
    body.write_string(email);
    body.write_string(phone);

    std::string packet = MessageHelper::pack_header(
        MsgType::REGISTER_REQUEST, body.data().size(), 1);
    packet.append(body.data());

    send_raw(packet);
}

bool Client::wait_response(MsgHeader& hdr, Buffer& body, int timeout_sec) {
    char buf[65536];
    
    // 设置超时
    struct timeval tv;
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;
    setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    while (true) {
        int n = recv(sockfd_, buf, sizeof(buf), 0);
        if (n <= 0) {
            return false;  // 超时或断开
        }
        recv_buffer_.append(buf, n);

        // 尝试解析
        if (MessageHelper::unpack_header(recv_buffer_, hdr)) {
            if (recv_buffer_.size() >= hdr.length) {
                // 提取包体
                std::string body_data = recv_buffer_.substr(
                    sizeof(MsgHeader), hdr.length - sizeof(MsgHeader));
                recv_buffer_ = recv_buffer_.substr(hdr.length);
                body = Buffer(body_data);
                return true;
            }
        }
    }
}
void Client::send_verify_code(const std::string& target, uint8_t type) {
    Buffer body;
    body.write_string(target);
    body.write_int8(type);
    std::string packet = MessageHelper::pack_header(MsgType::VERIFY_CODE_REQUEST, body.data().size(), 1);
    packet.append(body.data());
    send_raw(packet);
}

void Client::send_login_with_code(const std::string& target, const std::string& code) {
    Buffer body;
    body.write_string(target);
    body.write_string(code);
    std::string packet = MessageHelper::pack_header(MsgType::LOGIN_REQUEST, body.data().size(), 1);
    packet.append(body.data());
    send_raw(packet);
}
} // namespace chat