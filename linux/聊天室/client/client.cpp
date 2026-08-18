#include "../include/client/client.h"
#include <cstring>
#include <cerrno>
#include <poll.h>

namespace chat {


Client::Client(const std::string& host, int port) : host_(host), port_(port), sockfd_(-1) {
    
}


Client::~Client() { 
    disconnect(); 
}


bool Client::connect() {

//创建socket（AF_INET表示IPv4，SOCK_STREAM表示TCP协议）
    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);

//判断socket是否创建成功
    if (sockfd_ < 0) { 
        std::cerr << "创建socket失败\n"; 
        return false; 
    }
//存储服务器地址信息
    sockaddr_in addr; 

//初始化地址结构体，将addr结构体的所有字节设置为0，是为了确保结构体中的所有字段都被初始化为默认值，避免出现未定义行为从而导致程序错误或崩溃。
    memset(&addr, 0, sizeof(addr));
//录入地址信息
    addr.sin_family = AF_INET; //指定地址族为IPv4
    addr.sin_port = htons(port_);//将主机字节序转换为网络字节序（大端序），确保在不同平台之间传输数据时的正确性

    inet_pton(AF_INET, host_.c_str(), &addr.sin_addr);

    if (::connect(sockfd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "连接失败\n"; close(sockfd_); 
        sockfd_ = -1; 

        return false;
    }

    connected_ = true; 
    std::cout << "已连接\n"; 
    return true;
}


void Client::set_user_info(int64_t id, const std::string& username) {
    user_id_ = id;
    username_ = username;
}

void Client::disconnect() {
    running_ = false;

    if (recv_thread_.joinable()){
        recv_thread_.join();
    }

    if (sockfd_ >= 0) { 
        close(sockfd_); 
        sockfd_ = -1; 
    }

    connected_ = false;
}

void Client::send_raw(const std::string& data) {
    if (!connected_) {
        return ;
    }

    ::send(sockfd_, data.c_str(), data.size(), 0);

}

void Client::send_login(const std::string& u, const std::string& p) {
    Buffer body; 
    body.write_string(u); 
    body.write_string(p);

    std::string pkt = MessageHelper::pack_header(MsgType::LOGIN_REQUEST, body.data().size(), 1);

    pkt.append(body.data()); 
    send_raw(pkt);
}

void Client::send_register(const std::string& u, const std::string& p, const std::string& e, const std::string& ph) {
    Buffer body;
    body.write_string(u);
    body.write_string(p); 
    body.write_string(e); 
    body.write_string(ph);

    std::string pkt = MessageHelper::pack_header(MsgType::REGISTER_REQUEST, body.data().size(), 1);

    pkt.append(body.data()); 
    send_raw(pkt);
}

void Client::send_verify_code(const std::string& t, uint8_t tp) {
    Buffer body; 
    body.write_string(t); 
    body.write_int8(tp);

    std::string pkt = MessageHelper::pack_header(MsgType::VERIFY_CODE_REQUEST, body.data().size(), 1);
    
    pkt.append(body.data()); 
    send_raw(pkt);
}

void Client::send_login_with_code(const std::string& t, const std::string& c) {
    Buffer body; 
    body.write_string(t); 
    body.write_string(c);
    std::string pkt = MessageHelper::pack_header(MsgType::LOGIN_REQUEST, body.data().size(), 1);
    
    pkt.append(body.data()); 
    send_raw(pkt);
}

void Client::send_friend_add(const std::string& u) {
    Buffer body; 
    body.write_string(u);

    std::string pkt = MessageHelper::pack_header(MsgType::FRIEND_ADD_REQUEST, body.data().size(), 1);
    
    pkt.append(body.data()); 
    send_raw(pkt);
}

void Client::send_friend_delete(int64_t fid) {
    Buffer body; 
    body.write_int64(fid);
    
    std::string pkt = MessageHelper::pack_header(MsgType::FRIEND_DELETE_REQUEST, body.data().size(), 1);
    
    pkt.append(body.data()); 
    send_raw(pkt);
}

void Client::send_friend_list() {
    send_raw(MessageHelper::pack_header(MsgType::FRIEND_LIST_REQUEST, 0, 1));
}

void Client::send_friend_block(int64_t fid) {
    Buffer body; 
    body.write_int64(fid);
    
    std::string pkt = MessageHelper::pack_header(MsgType::FRIEND_BLOCK_REQUEST, body.data().size(), 1);
    
    pkt.append(body.data()); 
    send_raw(pkt);
}

void Client::send_friend_unblock(int64_t fid) {
    Buffer body; 
    body.write_int64(fid);
    
    std::string pkt = MessageHelper::pack_header(MsgType::FRIEND_UNBLOCK_REQUEST, body.data().size(), 1);
    
    pkt.append(body.data()); 
    send_raw(pkt);
}

void Client::send_private_message(int64_t to, const std::string& c) {
    Buffer body; 
    body.write_int64(to); 
    body.write_string(c);
    
    std::string pkt = MessageHelper::pack_header(MsgType::PRIVATE_MESSAGE, body.data().size(), 1);
    
    pkt.append(body.data()); 
    send_raw(pkt);
}

void Client::send_private_history(int64_t fid) {
    Buffer body; 
    body.write_int64(fid);

    send_raw(MessageHelper::pack_header(MsgType::PRIVATE_HISTORY_REQUEST, body.data().size(), 1) + body.data());
}

void Client::send_logout() {
    send_raw(MessageHelper::pack_header(MsgType::LOGOUT_REQUEST, 0, 1));
}

void Client::send_file_init(int64_t to, int64_t gid, const std::string& name, int64_t size) {
    Buffer body;
    body.write_int64(to);
    body.write_int64(gid);
    body.write_string(name);
    body.write_int64(size);

    std::string p = MessageHelper::pack_header(MsgType::FILE_TRANSFER_INIT, body.data().size(), 1);
    
    send_raw(p + body.data());
}

void Client::send_file_chunk(const std::string& tid, const std::string& data) {
    Buffer body;
    body.write_string(tid);
    body.write_string(data);

    std::string p = MessageHelper::pack_header(MsgType::FILE_TRANSFER_CHUNK, body.data().size(), 1);
    
    send_raw(p + body.data());
}

void Client::send_file_complete(const std::string& tid) {
    Buffer body;
    body.write_string(tid);

    std::string p = MessageHelper::pack_header(MsgType::FILE_TRANSFER_COMPLETE, body.data().size(), 1);
    
    send_raw(p + body.data());
}

bool Client::send_file(const std::string& filepath, int64_t to_id, int64_t group_id) {
    FILE* f = fopen(filepath.c_str(), "rb");
    if (!f) { std::cerr << "文件不存在\n"; return false; }

    fseek(f, 0, SEEK_END);
    int64_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::string fname = filepath;
    size_t pos = fname.find_last_of('/');

    if (pos != std::string::npos) {
        fname = fname.substr(pos + 1);
    }

    send_file_init(to_id, group_id, fname, size);

    std::string tid;
    while (true) {
        MsgType type; 
        Buffer body;

        if (!wait_msg(type, body, 10)) { 
            fclose(f); 
            return false; 
        }

        if (type == MsgType::FILE_TRANSFER_INIT_ACK) {
            tid = body.read_string();
            break;
        }
    }

    const size_t CHUNK = 65536;
    char buf[CHUNK];
    size_t n;

    while ((n = fread(buf, 1, CHUNK, f)) > 0) {
        send_file_chunk(tid, std::string(buf, n));
    }

    fclose(f);
    send_file_complete(tid);

    std::cout << "文件发送完成: " << tid << "\n";

    return true;
}

bool Client::download_file(const std::string& tid, const std::string& save_path) {
    Buffer req;

    req.write_string(tid);
    std::string pkt = MessageHelper::pack_header(MsgType::FILE_DOWNLOAD_REQUEST, req.data().size(), 1);
    send_raw(pkt + req.data());

    std::vector<std::pair<MsgType, std::string>> saved;

    while (true) {
        MsgType type; Buffer body;
        if (!wait_msg(type, body, 10)) {
            std::cerr << "下载超时：未收到服务器响应\n";
            for (auto& m : saved) {
                std::lock_guard<std::mutex> lk(queue_mutex_);
                msg_queue_.push({m.first, m.second});
            }
            return false;
        }

        if (type == MsgType::FILE_DOWNLOAD_RESPONSE) {
            body.read_string();
            std::string fname = body.read_string();
            body.read_int64();
            std::string data = body.read_string();

            for (auto& m : saved) {
                std::lock_guard<std::mutex> lk(queue_mutex_);
                msg_queue_.push({m.first, m.second});
            }
            saved.clear();

            std::string actual_path = save_path.empty() ? fname : save_path;

            FILE* f = fopen(actual_path.c_str(), "wb");
            if (!f) {
                std::cerr << "错误：无法创建文件 " << actual_path << ": " << strerror(errno) << "\n";
                return false;
            }
            size_t written = fwrite(data.data(), 1, data.size(), f);
            fclose(f);

            if (written == data.size()) {
                std::cout << "文件已保存: " << actual_path << " (" << written << "字节)\n";
            } else {
                std::cerr << "错误：写入不完整 " << written << "/" << data.size() << "\n";
                return false;
            }
            return true;
        }
        saved.push_back({type, body.data()});
    }
}


void Client::start_heartbeat() {
    // 心跳不再使用独立线程，合并到接收线程的轮询循环中统一调度。
    // 保留此函数与调用点，main.cpp 无需改动；重连时会再次调用以重置计时。
    last_heartbeat_ = time(nullptr);
}

void Client::start_recv_thread() {
    if (recv_thread_.joinable()) {
        recv_thread_.join();   // 重连前回收旧接收线程，避免 std::thread 赋值导致 terminate
    }
    running_ = true;
    recv_thread_ = std::thread([this]() {

        char buf[65536]; 
        std::string buffer;

        while (running_) {
            // 用 poll 带 1 秒超时等待可读：无数据时也能醒来检查心跳
            struct pollfd pfd;
            pfd.fd = sockfd_;
            pfd.events = POLLIN;
            int pr = poll(&pfd, 1, 1000);

            if (pr > 0) {
                if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    connected_ = false;
                    break;
                }
                if (pfd.revents & POLLIN) {
                    int n = recv(sockfd_, buf, sizeof(buf), 0);
                    if (n <= 0) { 
                        connected_ = false; 
                        break; 
                    }

                    buffer.append(buf, n);

                    while (buffer.size() >= sizeof(MsgHeader)) {
                        MsgHeader hdr;

                        memcpy(&hdr, buffer.data(), sizeof(MsgHeader));

                        if (buffer.size() < hdr.length) {
                            break;
                        }

                        std::string body = buffer.substr(sizeof(MsgHeader), hdr.length - sizeof(MsgHeader));
                        buffer = buffer.substr(hdr.length);
                        
                        std::lock_guard<std::mutex> lk(queue_mutex_);
                        msg_queue_.push({hdr.type, body});
                        queue_cv_.notify_one();
                    }
                }
            }
            else if (pr < 0) {
                if (errno == EINTR) continue;   // 被信号打断，重试
                connected_ = false;
                break;
            }

            // 心跳检查：距上次心跳 ≥ 30 秒就发送（服务端超时 60s，30s 足够安全）
            if (time(nullptr) - last_heartbeat_ >= 30) {
                send_raw(MessageHelper::pack_header(MsgType::HEARTBEAT, 0, 0));
                last_heartbeat_ = time(nullptr);
            }
        }
    });
}

//群组

void Client::send_group_create(const std::string& name, const std::string& desc) {
    Buffer body; 
    body.write_string(name); 
    body.write_string(desc);

    send_raw(MessageHelper::pack_header(MsgType::GROUP_CREATE_REQUEST, body.data().size(), 1) + body.data());

}

void Client::send_group_dissolve(int64_t gid) {
    Buffer body; 
    body.write_int64(gid);

    send_raw(MessageHelper::pack_header(MsgType::GROUP_DISSOLVE_REQUEST, body.data().size(), 1) + body.data());
}

void Client::send_group_join(int64_t gid, const std::string& msg) {
    Buffer body; 
    body.write_int64(gid); 
    body.write_string(msg);

    send_raw(MessageHelper::pack_header(MsgType::GROUP_JOIN_REQUEST, body.data().size(), 1) + body.data());
}

void Client::send_group_quit(int64_t gid) {
    Buffer body; 
    body.write_int64(gid);

    send_raw(MessageHelper::pack_header(MsgType::GROUP_QUIT_REQUEST, body.data().size(), 1) + body.data());
}

void Client::send_group_kick(int64_t gid, int64_t target_uid) {
    Buffer body; 
    body.write_int64(gid); 
    body.write_int64(target_uid);

    send_raw(MessageHelper::pack_header(MsgType::GROUP_KICK_REQUEST, body.data().size(), 1) + body.data());
}

void Client::send_group_info(int64_t gid) {
    Buffer body; 
    body.write_int64(gid);

    send_raw(MessageHelper::pack_header(MsgType::GROUP_INFO_REQUEST, body.data().size(), 1) + body.data());
}

void Client::send_group_list() {
    send_raw(MessageHelper::pack_header(MsgType::GROUP_LIST_REQUEST, 0, 1));
}

void Client::send_group_members(int64_t gid) {
    Buffer body; 
    body.write_int64(gid);

    send_raw(MessageHelper::pack_header(MsgType::GROUP_MEMBERS_REQUEST, body.data().size(), 1) + body.data());
}

void Client::send_group_message(int64_t gid, const std::string& content) {
    Buffer body; 
    body.write_int64(gid); 
    body.write_string(content);

    send_raw(MessageHelper::pack_header(MsgType::GROUP_MESSAGE, body.data().size(), 1) + body.data());
}

void Client::send_group_history(int64_t gid) {
    Buffer body; 
    body.write_int64(gid);

    send_raw(MessageHelper::pack_header(MsgType::GROUP_HISTORY_REQUEST, body.data().size(), 1) + body.data());
}

void Client::send_group_join_req_handle(int64_t req_id, uint8_t approve) {
    Buffer body; 
    body.write_int64(req_id); 
    body.write_int8(approve);

    send_raw(MessageHelper::pack_header(MsgType::GROUP_JOIN_REQ_HANDLE, body.data().size(), 1) + body.data());
}

bool Client::send_group_file(const std::string& filepath, int64_t gid) {
    return send_file(filepath, 0, gid);
}

void Client::send_group_set_admin(int64_t gid, int64_t target_uid, uint8_t set_admin) {
    Buffer body; 
    body.write_int64(gid); 
    body.write_int64(target_uid); 
    body.write_int8(set_admin);

    send_raw(MessageHelper::pack_header(MsgType::GROUP_SET_ADMIN, body.data().size(), 1) + body.data());
}


bool Client::wait_msg(MsgType& type, Buffer& body, int timeout_sec) {
    std::unique_lock<std::mutex> lk(queue_mutex_);
    if (!queue_cv_.wait_for(lk, std::chrono::seconds(timeout_sec),
        [this] { return !msg_queue_.empty() || !connected_; })) {
            return false;
        }

    if (msg_queue_.empty()) {
        return false;
    }

    type = msg_queue_.front().first;
    body = Buffer(msg_queue_.front().second);
    msg_queue_.pop();
    return true;
}

} // namespace chat