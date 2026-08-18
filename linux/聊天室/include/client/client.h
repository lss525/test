#ifndef CHAT_CLIENT_H
#define CHAT_CLIENT_H

#include <string>
#include <cstdint>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <thread>
#include <atomic>
#include "../common/message.h"

namespace chat {

class Client {
public:
    Client(const std::string& host, int port);
    ~Client();

    bool connect();
    void disconnect();
    bool is_connected() const { return connected_; }

    void send_raw(const std::string& data);
    void send_login(const std::string& username, const std::string& password);
    void send_register(const std::string& username, const std::string& password,
                       const std::string& email, const std::string& phone);
    void send_verify_code(const std::string& target, uint8_t type);
    void send_login_with_code(const std::string& target, const std::string& code);

    // 好友
    void send_friend_add(const std::string& target_name);
    void send_friend_delete(int64_t fid);
    void send_friend_list();
    void send_friend_block(int64_t fid);
    void send_friend_unblock(int64_t fid);

    // 私聊
    void send_private_message(int64_t to_id, const std::string& content);
    void send_private_history(int64_t fid);

    // 文件
    void send_file_init(int64_t to_id, int64_t gid, const std::string& fname, int64_t fsize);
    void send_file_chunk(const std::string& tid, const std::string& data);
    void send_file_complete(const std::string& tid);
    bool send_file(const std::string& file_path, int64_t to_id, int64_t gid);
    bool download_file(const std::string& tid, const std::string& save_path);

    // 群组
    void send_group_create(const std::string& name, const std::string& desc);
    void send_group_dissolve(int64_t gid);
    void send_group_join(int64_t gid, const std::string& msg);
    void send_group_quit(int64_t gid);
    void send_group_kick(int64_t gid, int64_t target_uid);
    void send_group_info(int64_t gid);
    void send_group_list();
    void send_group_members(int64_t gid);
    void send_group_message(int64_t gid, const std::string& content);
    void send_group_history(int64_t gid);
    void send_group_join_req_handle(int64_t req_id, uint8_t approve);
    bool send_group_file(const std::string& file_path, int64_t gid);
    void send_group_set_admin(int64_t gid, int64_t target_uid, uint8_t set_admin);

    void send_logout();
    void set_user_info(int64_t id, const std::string& username);
    int64_t get_user_id() const { return user_id_; }
    std::string get_username() const { return username_; }

    void start_heartbeat();
    void start_recv_thread();
    bool wait_msg(MsgType& type, Buffer& body, int timeout_sec);

private:
    std::string host_;
    int port_;
    int sockfd_;
    std::atomic<bool> connected_{false};
    std::string recv_buffer_;
    std::atomic<bool> running_{false};
    std::thread recv_thread_;
    std::atomic<time_t> last_heartbeat_{0};   // 上次发送心跳的时间（心跳合并进接收线程）

    int64_t user_id_ = 0;
    std::string username_;

    std::queue<std::pair<MsgType, std::string>> msg_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
};

} // namespace chat

#endif
