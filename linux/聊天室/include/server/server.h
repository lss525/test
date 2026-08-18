#ifndef CHAT_SERVER_H
#define CHAT_SERVER_H

#include <memory>
#include <atomic>
#include <map>
#include <mutex>
#include "reactor.h"
#include "connection.h"
#include "../datebase/mysql_pool.h"
#include "../datebase/redis_pool.h"


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

    // 文件传输：transfer_id → 已打开的文件句柄（复用，避免每片 open/close）
    std::map<std::string, int> file_fds_;
    std::mutex file_fds_mutex_;

    
    //登陆等
    void handle_login(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void handle_register(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void handle_logout(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void handle_verify_code(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void handle_password_reset(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void handle_heartbeat(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    
    // 好友
    void handle_friend_add(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void handle_friend_delete(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void handle_friend_list(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void handle_friend_block(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void handle_friend_unblock(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);

    // 私聊
    void handle_private_message(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void handle_private_history(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);

    //文件
    void handle_file_transfer_init(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void handle_file_transfer_chunk(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void handle_file_transfer_complete(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void send_offline_files(int64_t uid);
    void send_offline_group_messages(int64_t uid);
    void send_offline_group_files(int64_t uid);
    void handle_file_download(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);

    //  群组 
    void handle_group_create(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void handle_group_dissolve(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void handle_group_join(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void handle_group_quit(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void handle_group_kick(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void handle_group_info(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void handle_group_list(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void handle_group_members(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void handle_group_message(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void handle_group_history(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void handle_group_join_req_handle(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);
    void send_offline_group_join_requests(int64_t uid);
    void handle_group_set_admin(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body);

    bool is_group_member(int64_t gid, int64_t uid);
    int  get_group_role(int64_t gid, int64_t uid);
    void send_to_group(int64_t gid, int64_t exclude_uid, const std::string& data);
    void send_group_notify(int64_t gid, const std::string& msg);
    std::vector<int64_t> get_group_member_ids(int64_t gid);
    std::string get_username(int64_t uid);
    
    // 辅助
    bool is_friend(int64_t uid, int64_t fid);
    bool is_blocked(int64_t uid, int64_t fid);
    void send_to_user(int64_t uid, const std::string& data);
    void send_offline_messages(int64_t uid);
    void notify_friend_status(int64_t uid, int status);

};


} // namespace chat

#endif