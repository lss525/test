#ifndef CHAT_CONNECTION_H
#define CHAT_CONNECTION_H

#include <string>
#include <memory>
#include <mutex>
#include "/home/lighning/codes/test/linux/聊天室/include/common/user.h"
#include "/home/lighning/codes/test/linux/聊天室/include/common/message.h"

namespace chat {

class SubReactor;
class Server;

class Connection : public std::enable_shared_from_this<Connection> {
public:
    Connection(int fd, SubReactor* reactor, Server* server);
    ~Connection();

    int get_fd() const { return fd_; }
    void handle_read();
    void handle_write();
    void handle_close();

    void send_raw(const std::string& data);
    void set_user(const UserInfo& user) { user_ = user; authenticated_ = true; }
    int64_t get_user_id() const { return user_.id; }
    const UserInfo& get_user_info() const { return user_; }
    bool is_authenticated() const { return authenticated_; }
    void update_heartbeat() { last_heartbeat_ = time(nullptr); }
    time_t get_last_heartbeat() const { return last_heartbeat_; }

private:
    int fd_;
    SubReactor* reactor_;
    Server* server_;
    UserInfo user_;
    bool authenticated_ = false;
    time_t last_heartbeat_;
    std::string read_buffer_;
    std::string write_buffer_;
    std::mutex write_mutex_;
    bool writing_ = false;

    void do_write();
    void process_packet();
};

} // namespace chat

#endif