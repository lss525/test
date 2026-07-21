#ifndef CHAT_SERVER_H
#define CHAT_SERVER_H

#include <memory>
#include <atomic>
#include <map>
#include <mutex>
#include "reactor.h"
#include "connection.h"
#include "/home/lighning/codes/test/linux/聊天室/include/datebase/mysql_pool.h"
#include "/home/lighning/codes/test/linux/聊天室/include/datebase/redis_pool.h"

namespace chat {

class Server {
public:
    Server(int port, const std::string& address);
    ~Server();

    bool initialize();
    void run();
    void stop();

    void on_new_connection(int fd);
    void on_connection_close(int fd);
    void register_user_conn(int64_t user_id, std::shared_ptr<Connection> conn);
    void unregister_user_conn(int64_t user_id);

    MySQLPool* mysql() { return mysql_pool_.get(); }
    RedisPool* redis() { return redis_pool_.get(); }

    void send_error(std::shared_ptr<Connection> conn, uint32_t seq, const std::string& msg);
    void check_heartbeat();
    void dispatch(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
private:
    int port_;
    std::string address_;
    std::atomic<bool> running_{false};

    std::unique_ptr<MainReactor> main_reactor_;
    std::vector<std::unique_ptr<SubReactor>> sub_reactors_;
    std::unique_ptr<WorkerPool> worker_pool_;
    std::unique_ptr<HeartbeatTimer> heartbeat_timer_;
    std::unique_ptr<MySQLPool> mysql_pool_;
    std::unique_ptr<RedisPool> redis_pool_;

    std::map<int64_t, std::shared_ptr<Connection>> connections_;
    std::mutex conn_mutex_;
    int next_reactor_ = 0;

    

    void handle_login(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void handle_register(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void handle_logout(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void handle_verify_code(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void handle_password_reset(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void handle_heartbeat(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
};

} // namespace chat

#endif