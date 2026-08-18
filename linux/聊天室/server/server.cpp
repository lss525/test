#include "../include/server/server.h"
#include "../include/server/logger.h"
#include "../include/common/message.h"
#include "../include/common/crypto.h"
#include "../include/common/config.h"
#include <chrono>
#include "../include/common/email.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>
#include <mysql/mysql.h>


namespace chat {

namespace {
std::string esc(MYSQL* m, const std::string& s) {
    if (s.empty() || !m) return s;
    std::string out(s.size() * 2 + 1, '\0');
    unsigned long len = mysql_real_escape_string(m, &out[0], s.c_str(), s.size());
    out.resize(len);
    return out;
}
} // namespace


Server::Server(int port, const std::string& address) : port_(port), address_(address) {

}

Server::~Server() { 
    stop(); 
}

bool Server::initialize() {
    try {

        mysql_pool_ = std::make_unique<MySQLPool>(
            Config::get("mysql.host", "127.0.0.1"),
            Config::get_int("mysql.port", 3306),
            Config::get("mysql.user", "chat"),
            Config::get("mysql.password", ""),
            Config::get("mysql.database", "chat_system"),
            20);

        redis_pool_ = std::make_unique<RedisPool>(
            Config::get("redis.host", "127.0.0.1"),
            Config::get_int("redis.port", 6379),
            Config::get("redis.password", ""),
            Config::get_int("redis.db", 0),
            10);
        
        auto* m = mysql_pool_->acquire();
        mysql_query(m, "SET NAMES utf8mb4");
        mysql_query(m, "UPDATE users SET status = 0");
        mysql_pool_->release(m);


        heartbeat_timer_ = std::make_unique<HeartbeatTimer>(60);
        worker_pool_ = std::make_unique<WorkerPool>(4);

        for (int i = 0; i < 4; ++i) {
            sub_reactors_.push_back(std::make_unique<SubReactor>(i));
        }

        main_reactor_ = std::make_unique<MainReactor>(this, port_, address_);

        LOG_INFO("Server initialized");

        return true;

    } 
    catch (const std::exception& e) {
        LOG_ERROR("Init failed: {}", e.what());

        return false;
    }
}

void Server::run() {
    running_ = true;
    worker_pool_->start();

    for (auto& r : sub_reactors_) {
        r->run();
    }
    main_reactor_->run();
    
    // 阻塞等待主线程
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

}

void Server::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    worker_pool_->stop();

    for (auto& r : sub_reactors_) {
        r->stop();
    }

    main_reactor_->stop();
}

void Server::on_new_connection(int fd) {
    int idx = next_reactor_++ % sub_reactors_.size();

    auto conn = std::make_shared<Connection>(fd, sub_reactors_[idx].get(), this);

    sub_reactors_[idx]->add_connection(conn);
    heartbeat_timer_->update(fd);

    LOG_INFO("New connection fd={}", fd);
}

void Server::on_connection_close(int fd) {

    heartbeat_timer_->remove(fd);
    std::lock_guard<std::mutex> lock(conn_mutex_);

    for (auto it = connections_.begin(); it != connections_.end(); ++it) {
        if (it->second && it->second->get_fd() == fd) {

            int64_t uid = it->first;

            connections_.erase(it);

            auto* m = mysql_pool_->acquire();

            std::string q = "UPDATE users SET status = 0 WHERE id = " + std::to_string(uid);
            mysql_query(m, q.c_str());

            mysql_pool_->release(m);
            redis_pool_->del("online:" + std::to_string(uid));

            notify_friend_status(uid, 0);
            LOG_INFO("User {} disconnected", uid);
            return;
        }
    }
    close(fd);
    LOG_INFO("Unauthenticated connection closed fd={}", fd);
}

void Server::register_user_conn(int64_t uid, std::shared_ptr<Connection> conn) {

    std::lock_guard<std::mutex> lock(conn_mutex_);

    connections_[uid] = conn;
}

void Server::unregister_user_conn(int64_t uid) {
    std::lock_guard<std::mutex> lock(conn_mutex_);

    connections_.erase(uid);
}

void Server::check_heartbeat() {
    for (int fd : heartbeat_timer_->check_timeout()) {

        LOG_INFO("Heartbeat timeout fd={}", fd);

        on_connection_close(fd);
    }
}

void Server::send_error(std::shared_ptr<Connection> conn, uint32_t seq, const std::string& msg) {
    std::string body;
    body.append((char*)&seq, 4);

    uint32_t mlen = msg.size();

    body.append((char*)&mlen, 4);
    body.append(msg);

    std::string packet = MessageHelper::pack_header(MsgType::ERROR_RESPONSE, body.size(), seq);
    packet.append(body);

    conn->send_raw(packet);
}

void Server::dispatch(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {

    // printf("recv type=%u\n", (uint16_t)hdr.type);

    switch (hdr.type) {
        //登陆认证
        case MsgType::LOGIN_REQUEST:         
        handle_login(conn, hdr, body); 
        break;
        case MsgType::REGISTER_REQUEST:      
        handle_register(conn, hdr, body); 
        break;
        case MsgType::LOGOUT_REQUEST:        
        handle_logout(conn, hdr, body); 
        break;
        case MsgType::VERIFY_CODE_REQUEST:   
        handle_verify_code(conn, hdr, body); 
        break;
        case MsgType::PASSWORD_RESET_REQUEST:
        handle_password_reset(conn, hdr, body); 
        break;

        //好友
        case MsgType::FRIEND_ADD_REQUEST:    
        handle_friend_add(conn, hdr, body); 
        break;
        case MsgType::FRIEND_DELETE_REQUEST: 
        handle_friend_delete(conn, hdr, body); 
        break;
        case MsgType::FRIEND_LIST_REQUEST:   
        handle_friend_list(conn, hdr, body); 
        break;
        case MsgType::FRIEND_BLOCK_REQUEST:  
        handle_friend_block(conn, hdr, body); 
        break;
        case MsgType::FRIEND_UNBLOCK_REQUEST:  
        handle_friend_unblock(conn, hdr, body); 
        break; 
        
        //私聊
        case MsgType::PRIVATE_MESSAGE:       
        handle_private_message(conn, hdr, body); 
        break;
        case MsgType::PRIVATE_HISTORY_REQUEST:
        handle_private_history(conn, hdr, body); 
        break;
        
        //文件
        case MsgType::FILE_TRANSFER_INIT:     
        handle_file_transfer_init(conn, hdr, body); 
        break;
        case MsgType::FILE_TRANSFER_CHUNK:    
        handle_file_transfer_chunk(conn, hdr, body); 
        break;
        case MsgType::FILE_TRANSFER_COMPLETE: 
        handle_file_transfer_complete(conn, hdr, body); 
        break;
        case MsgType::FILE_DOWNLOAD_REQUEST: 
        handle_file_download(conn, hdr, body); 
        break;
        
        //群管理
        case MsgType::GROUP_CREATE_REQUEST:   
        handle_group_create(conn, hdr, body); 
        break;
        case MsgType::GROUP_DISSOLVE_REQUEST: 
        handle_group_dissolve(conn, hdr, body); 
        break;
        case MsgType::GROUP_JOIN_REQUEST:     
        handle_group_join(conn, hdr, body); 
        break;
        case MsgType::GROUP_QUIT_REQUEST:     
        handle_group_quit(conn, hdr, body); 
        break;
        case MsgType::GROUP_KICK_REQUEST:     
        handle_group_kick(conn, hdr, body); 
        break;
        case MsgType::GROUP_INFO_REQUEST:     
        handle_group_info(conn, hdr, body); 
        break;
        case MsgType::GROUP_LIST_REQUEST:     
        handle_group_list(conn, hdr, body); 
        break;
        case MsgType::GROUP_MEMBERS_REQUEST:  
        handle_group_members(conn, hdr, body); 
        break;
        case MsgType::GROUP_MESSAGE:          
        handle_group_message(conn, hdr, body); 
        break;
        case MsgType::GROUP_HISTORY_REQUEST:  
        handle_group_history(conn, hdr, body); 
        break;
        case MsgType::GROUP_SET_ADMIN:  
        handle_group_set_admin(conn, hdr, body); 
        break;
        case MsgType::GROUP_JOIN_REQ_HANDLE:  
        handle_group_join_req_handle(conn, hdr, body); 
        break;
        
        //心跳监测
        case MsgType::HEARTBEAT:             
        handle_heartbeat(conn, hdr, body); 
        break;

        default: 
        break;
    }
}

// LOGIN 
void Server::handle_login(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {
    std::string username = body.read_string();
    std::string password = body.read_string();

    uint32_t seq = hdr.sequence;

    worker_pool_->submit([this, conn, username, password, seq]() {

        auto* m = mysql_pool_->acquire();

        uint8_t success = 0;
        uint64_t uid = 0;

        std::string nick, email, phone, uname;

        // 判断是否是6位纯数字（验证码）
        bool is_code = (password.length() == 6 && 
                        password.find_first_not_of("0123456789") == std::string::npos);

        if (is_code && username.find('@') != std::string::npos) {
    
            std::string q = "SELECT id, username, email, phone, nickname FROM users WHERE email='"
                            + username + "'";

            mysql_query(m, q.c_str());

            MYSQL_RES* res = mysql_store_result(m);
            MYSQL_ROW row = mysql_fetch_row(res);

            if (row) {
                // 验证验证码
                std::string q2 = "SELECT COUNT(*) FROM verification_codes WHERE target='"
                                + username + "' AND code='" + password 
                                + "' AND used=0 AND type=2 AND expires_at>" 
                                + std::to_string(time(nullptr));

                mysql_query(m, q2.c_str());

                MYSQL_RES* res2 = mysql_store_result(m);
                MYSQL_ROW row2 = mysql_fetch_row(res2);

                if (row2 && std::stoi(row2[0]) > 0) {
                    // 标记验证码已用
                    std::string q3 = "UPDATE verification_codes SET used=1 WHERE target='"
                                    + username + "' AND code='" + esc(m, password) + "'";
                    mysql_query(m, q3.c_str());
                    
                    success = 1;
                    uid = std::stoll(row[0]);

                    uname = row[1] ? row[1] : "";
                    email = row[2] ? row[2] : "";
                    phone = row[3] ? row[3] : "";
                    nick = row[4] ? row[4] : "";

                }
                mysql_free_result(res2);
            }

            mysql_free_result(res);

        } 
        else {
            // ===== 密码登录 =====
            std::string q = "SELECT id, username, password_hash, email, phone, nickname "
                            "FROM users WHERE username='" + esc(m, username) + "'";

            mysql_query(m, q.c_str());

            MYSQL_RES* res = mysql_store_result(m);
            MYSQL_ROW row = mysql_fetch_row(res);

            if (row) {

                std::string hash = row[2] ? row[2] : "";

                if (Crypto::verify_password(password, hash)) {
                    success = 1;
                    uid = std::stoll(row[0]);

                    uname = row[1] ? row[1] : "";
                    email = row[3] ? row[3] : "";
                    phone = row[4] ? row[4] : "";
                    nick = row[5] ? row[5] : "";
                }
            }

            mysql_free_result(res);

        }

        // 登录成功
        if (success) {
           UserInfo user;

           user.id = uid; user.username = uname; user.email = email;
           user.phone = phone; user.nickname = nick; user.status = 1;

           conn->set_user(user);
           register_user_conn(uid, conn);

           std::string q = "UPDATE users SET status = 1 WHERE id = " + std::to_string(uid);
           mysql_query(m, q.c_str());

           redis_pool_->set("online:" + std::to_string(uid), "1", 3600);

           LOG_INFO("User '{}' logged in", uname);

           // 通知好友上线 & 发送离线消息
           notify_friend_status(uid, 1);
           send_offline_messages(uid);
           send_offline_group_messages(uid);
           send_offline_files(uid);
           send_offline_group_files(uid);
           send_offline_group_join_requests(uid);

        } 

        mysql_pool_->release(m);

        // 打包响应
        Buffer resp;

        resp.write_int8(success);
        resp.write_int64(uid);
        resp.write_string(uname);
        resp.write_string(nick);
        resp.write_string(email);
        resp.write_string(phone);

        std::string packet = MessageHelper::pack_header(MsgType::LOGIN_RESPONSE, resp.data().size(), seq);
        packet.append(resp.data());
        
        conn->send_raw(packet);
    });
}


void Server::handle_register(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {

    std::string username = body.read_string();
    std::string password = body.read_string();
    std::string email = body.read_string();
    std::string phone = body.read_string();

    uint32_t seq = hdr.sequence;

    worker_pool_->submit([this, conn, username, password, email, phone, seq]() {
        auto* m = mysql_pool_->acquire();

        std::string q = "SELECT COUNT(*) FROM users WHERE username='" + esc(m, username) + "'";

        mysql_query(m, q.c_str());
        MYSQL_RES* res = mysql_store_result(m);

        if (!res) { 
            mysql_pool_->release(m); 
            return; 
        };

        MYSQL_ROW row = mysql_fetch_row(res);

        uint8_t success = 0;
        uint64_t uid = 0;

        if (row && std::stoi(row[0]) > 0) {

        }
        else {
            std::string hash = Crypto::hash_password(password);

            q = "INSERT INTO users (username, password_hash, email, phone, created_at) VALUES ('"
                + username + "','" + esc(m, hash) + "','" + esc(m, email) + "','" + esc(m, phone) + "',"
                + std::to_string(time(nullptr)) + ")";

            mysql_query(m, q.c_str());

            uid = mysql_insert_id(m);

            success = 1;

            LOG_INFO("User '{}' registered", username);

        }

        mysql_free_result(res);

        mysql_pool_->release(m);

        Buffer resp;
        resp.write_int8(success);
        resp.write_int64(uid);

        std::string packet = MessageHelper::pack_header(MsgType::REGISTER_RESPONSE, resp.data().size(), seq);
        packet.append(resp.data());

        conn->send_raw(packet);
    });
}

//  LOGOUT 
void Server::handle_logout(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer&) {
    if (!conn->is_authenticated()) {
        return;
    }
    int64_t uid = conn->get_user_id();

    notify_friend_status(uid, 0); 

    auto* m = mysql_pool_->acquire();

    std::string q = "UPDATE users SET status = 0 WHERE id = " + std::to_string(uid);

    mysql_query(m, q.c_str());
    mysql_pool_->release(m);

    redis_pool_->del("online:" + std::to_string(uid));
    unregister_user_conn(uid);

    Buffer resp;
    resp.write_int8(1);
    std::string packet = MessageHelper::pack_header(MsgType::LOGOUT_RESPONSE, resp.data().size(), hdr.sequence);

    packet.append(resp.data());
    conn->send_raw(packet);

    LOG_INFO("User {} logged out", uid);

}

//  VERIFY CODE 
void Server::handle_verify_code(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {

    std::string target = body.read_string();
    uint8_t code_type = body.read_int8();

    std::string code = Crypto::generate_verify_code();
    uint32_t seq = hdr.sequence;

    // 1. 先存验证码到数据库（同步快速完成）
    auto* m = mysql_pool_->acquire();

    std::string q = "INSERT INTO verification_codes (target, code, type, expires_at) VALUES ('"
                    + target + "','" + esc(m, code) + "'," + std::to_string(code_type) + ","
                    + std::to_string(time(nullptr) + 300) + ")";

    mysql_query(m, q.c_str());
    mysql_pool_->release(m);

    // 2. 马上回复客户端成功
    Buffer resp;

    resp.write_int8(1);
    std::string packet = MessageHelper::pack_header(MsgType::VERIFY_CODE_RESPONSE, resp.data().size(), seq);

    packet.append(resp.data());
    conn->send_raw(packet);

    // 3. 异步发邮件（不阻塞响应）
    worker_pool_->submit([target, code]() {
        EmailSender::send_code(target, code);
    });
}

//  PASSWORD RESET
void Server::handle_password_reset(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {
    
    std::string target = body.read_string();
    std::string code = body.read_string();
    std::string new_password = body.read_string();

    auto* m = mysql_pool_->acquire();
    std::string q = "SELECT COUNT(*) FROM verification_codes WHERE target='" + target
                    + "' AND code='" + esc(m, code) + "' AND used=0 AND expires_at>" + std::to_string(time(nullptr));

    mysql_query(m, q.c_str());
    MYSQL_RES* res = mysql_store_result(m);

    MYSQL_ROW row = mysql_fetch_row(res);
    uint8_t success = 0;

    if (row && std::stoi(row[0]) > 0) {
        std::string hash = Crypto::hash_password(new_password);
        q = "UPDATE users SET password_hash='" + esc(m, hash) + "' WHERE email='" + esc(m, target) + "' OR phone='" + esc(m, target) + "'";

        mysql_query(m, q.c_str());
        q = "UPDATE verification_codes SET used=1 WHERE target='" + esc(m, target) + "' AND code='" + esc(m, code) + "'";

        mysql_query(m, q.c_str());
        success = 1;

        LOG_INFO("Password reset for {}", target);
    }

    mysql_free_result(res);
    mysql_pool_->release(m);

    Buffer resp;
    resp.write_int8(success);

    std::string packet = MessageHelper::pack_header(MsgType::PASSWORD_RESET_RESPONSE, resp.data().size(), hdr.sequence);
    packet.append(resp.data());
    
    conn->send_raw(packet);
}

// FRIEND
bool Server::is_friend(int64_t uid, int64_t fid) {
    auto* m = mysql_pool_->acquire();

    std::string q = "SELECT COUNT(*) FROM friendships WHERE "
                    "((user_id=" + std::to_string(uid) + " AND friend_id=" + std::to_string(fid) + ") "
                    "OR (user_id=" + std::to_string(fid) + " AND friend_id=" + std::to_string(uid) + ")) "
                    "AND status=1";

    mysql_query(m, q.c_str());
    MYSQL_RES* res = mysql_store_result(m);

    if (!res) {
        mysql_pool_->release(m); 
        return false; 
    }

    MYSQL_ROW row = mysql_fetch_row(res);

    int count = row ? std::stoi(row[0]) : 0;

    mysql_free_result(res);
    mysql_pool_->release(m);

    return count > 0;
}

bool Server::is_blocked(int64_t uid, int64_t fid) {
    auto* m = mysql_pool_->acquire();

    std::string q = "SELECT COUNT(*) FROM friendships WHERE status=3 "
                    "AND blocked_by=" + std::to_string(fid) + " "
                    "AND ((user_id=" + std::to_string(uid) + " AND friend_id=" + std::to_string(fid) + ") "
                    "OR (user_id=" + std::to_string(fid) + " AND friend_id=" + std::to_string(uid) + "))";

    mysql_query(m, q.c_str());

    MYSQL_RES* res = mysql_store_result(m);

    if (!res) { 
        mysql_pool_->release(m); 
        return false; 
    }

    MYSQL_ROW row = mysql_fetch_row(res);

    int count = row ? std::stoi(row[0]) : 0;

    mysql_free_result(res);
    mysql_pool_->release(m);

    return count > 0;
}

void Server::send_to_user(int64_t uid, const std::string& data) {
    // std::lock_guard<std::mutex> lock(conn_mutex_);
    // printf("send_to_user uid=%ld, conns=%zu\n", uid, connections_.size());
    // auto it = connections_.find(uid);
    // if (it != connections_.end()) {
    //     it->second->send_raw(data);
    // } else {
    //     printf("NOT FOUND uid=%ld\n", uid);
    // }

    std::lock_guard<std::mutex> lock(conn_mutex_);

    auto it = connections_.find(uid);

    if (it != connections_.end() && it->second) {
        it->second->send_raw(data);
    }

}

void Server::send_offline_messages(int64_t uid) {
    auto* m = mysql_pool_->acquire();

    std::string q = "SELECT pm.message_id, pm.sender_id, u.username, pm.content, pm.message_type, pm.sent_at "
                    "FROM private_messages pm JOIN users u ON pm.sender_id=u.id "
                    "WHERE pm.receiver_id=" + std::to_string(uid) + " AND pm.status=0";

    if (mysql_query(m, q.c_str()) != 0) { 
        mysql_pool_->release(m); 
        return; 
    }

    MYSQL_RES* res = mysql_store_result(m);

    if (!res) { 
        mysql_pool_->release(m); 
        return; 
    }

    MYSQL_ROW row;

    while ((row = mysql_fetch_row(res))) {

        Buffer body;
        body.write_string(row[0] ? row[0] : "");   // message_id
        body.write_int64(row[1] ? std::stoll(row[1]) : 0);   // sender_id
        body.write_string(row[2] ? row[2] : "");   // username
        body.write_string(row[3] ? row[3] : "");   // content
        body.write_int64(row[5] ? std::stoll(row[5]) : 0);   // sent_at

        std::string packet = MessageHelper::pack_header(MsgType::OFFLINE_MESSAGE_NOTIFY, body.data().size(), 0);
        packet.append(body.data());

        send_to_user(uid, packet);
    }

    mysql_free_result(res);
    q = "UPDATE private_messages SET status=1 WHERE receiver_id=" + std::to_string(uid) + " AND status=0";

    mysql_query(m, q.c_str());
    
}

void Server::send_offline_group_messages(int64_t uid) {
    auto* m = mysql_pool_->acquire();
    std::string q = "SELECT gm.message_id, gm.group_id, gm.sender_id, u.username, gm.content, gm.sent_at "
                    "FROM group_messages gm "
                    "JOIN users u ON gm.sender_id=u.id "
                    "JOIN group_members gmem ON gm.group_id=gmem.group_id AND gmem.user_id=" + std::to_string(uid) + " "
                    "WHERE gm.sender_id !=" + std::to_string(uid) + " AND gm.status=0 "
                    "ORDER BY gm.sent_at DESC LIMIT 50";

    if (mysql_query(m, q.c_str()) != 0) { 
        mysql_pool_->release(m); 
        return; 
    }

    MYSQL_RES* res = mysql_store_result(m);

    if (!res) { 
        mysql_pool_->release(m); 
        return; 
    }
    
    MYSQL_ROW row;

    while ((row = mysql_fetch_row(res))) {
        Buffer body;
        body.write_int64(row[1] ? std::stoll(row[1]) : 0);
        body.write_int64(row[2] ? std::stoll(row[2]) : 0);
        body.write_string(row[3] ? row[3] : "");
        body.write_string(row[4] ? row[4] : "");
        body.write_int64(row[5] ? std::stoll(row[5]) : 0);

        std::string packet = MessageHelper::pack_header(MsgType::GROUP_MESSAGE, body.data().size(), 0);
        packet.append(body.data());

        send_to_user(uid, packet);
    }

    mysql_free_result(res);
    
    q = "UPDATE group_messages SET status=1 WHERE group_id IN "
        "(SELECT group_id FROM group_members WHERE user_id=" + std::to_string(uid) + ") "
        "AND sender_id !=" + std::to_string(uid) + " AND status=0";

    mysql_query(m, q.c_str());
    mysql_pool_->release(m);

}

void Server::send_offline_group_files(int64_t uid) {
    auto* m = mysql_pool_->acquire();

    std::string q = "SELECT ft.transfer_id, ft.sender_id, ft.file_name, ft.file_size, ft.group_id "
                    "FROM file_transfers ft "
                    "JOIN group_members gm ON ft.group_id=gm.group_id AND gm.user_id=" + std::to_string(uid) + " "
                    "WHERE ft.group_id>0 AND ft.status=1 AND ft.notified=0";

    if (mysql_query(m, q.c_str()) != 0) { 
        mysql_pool_->release(m); 
        return; 
    }

    MYSQL_RES* res = mysql_store_result(m);

    if (!res) { 
        mysql_pool_->release(m); 
        return; 
    }
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        Buffer body;
        body.write_string(row[0] ? row[0] : "");              // transfer_id
        body.write_int64(row[1] ? std::stoll(row[1]) : 0);    // sender_id
        body.write_string(row[2] ? row[2] : "");              // file_name
        body.write_int64(row[3] ? std::stoll(row[3]) : 0);    // file_size
        body.write_int64(row[4] ? std::stoll(row[4]) : 0);    // group_id

        std::string packet = MessageHelper::pack_header(MsgType::FILE_TRANSFER_INIT, body.data().size(), 0);
        
        packet.append(body.data());
        send_to_user(uid, packet);
    }

    mysql_free_result(res);
    // 标记已通知
    q = "UPDATE file_transfers SET notified=1 WHERE group_id IN "
        "(SELECT group_id FROM group_members WHERE user_id=" + std::to_string(uid) + ") "
        "AND status=1 AND notified=0";

    mysql_query(m, q.c_str());

    mysql_pool_->release(m);
}

void Server::send_offline_group_join_requests(int64_t uid) {
    auto* m = mysql_pool_->acquire();

    std::string q = "SELECT gr.id, gr.group_id, gr.user_id, u.username, gr.message, gr.created_at "
                    "FROM group_join_requests gr "
                    "JOIN users u ON gr.user_id=u.id "
                    "JOIN groups_info g ON gr.group_id=g.id "
                    "WHERE g.owner_id=" + std::to_string(uid) + " AND gr.status=0";

    q += " UNION "
         "SELECT gr.id, gr.group_id, gr.user_id, u.username, gr.message, gr.created_at "
         "FROM group_join_requests gr "
         "JOIN users u ON gr.user_id=u.id "
         "JOIN group_members gm ON gr.group_id=gm.group_id AND gm.user_id=" + std::to_string(uid) + " "
         "WHERE gm.role>=1 AND gr.status=0 AND gr.user_id!=" + std::to_string(uid);
    
    if (mysql_query(m, q.c_str()) != 0) { 
        mysql_pool_->release(m); 
        return; 
    }

    MYSQL_RES* res = mysql_store_result(m);

    if (!res) { 
        mysql_pool_->release(m); 
        return; 
    }
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        Buffer body;
        body.write_int64(row[1] ? std::stoll(row[1]) : 0);
        body.write_int64(row[0] ? std::stoll(row[0]) : 0);
        body.write_int64(row[2] ? std::stoll(row[2]) : 0);
        body.write_string(row[3] ? row[3] : "");
        body.write_string(row[4] ? row[4] : "");

        std::string np = MessageHelper::pack_header(MsgType::GROUP_JOIN_REQ_NOTIFY, body.data().size(), 0);  
        np.append(body.data());

        send_to_user(uid, np);
    }
    mysql_free_result(res);

    q = "SELECT gr.group_id FROM group_join_requests gr "
    "WHERE gr.user_id=" + std::to_string(uid) + " AND gr.status=2 AND gr.notified=0";

    if (mysql_query(m, q.c_str()) == 0) {

        MYSQL_RES* res2 = mysql_store_result(m);

        if (res2) {
            MYSQL_ROW row2;

            while ((row2 = mysql_fetch_row(res2))) {
                Buffer body;
                body.write_int64(row2[0] ? std::stoll(row2[0]) : 0);
                body.write_string("你的加群申请已被拒绝");

                std::string np = MessageHelper::pack_header(MsgType::GROUP_NOTIFY, body.data().size(), 0);
                
                np.append(body.data());

                send_to_user(uid, np);
            }
            
            mysql_free_result(res2);

        }

        mysql_query(m, ("UPDATE group_join_requests SET notified=1 WHERE user_id=" + std::to_string(uid) + " AND status=2 AND notified=0").c_str());
    
    }

    mysql_pool_->release(m);

}

void Server::notify_friend_status(int64_t uid, int status) {
    auto* m = mysql_pool_->acquire();
    std::string q = "SELECT friend_id FROM friendships WHERE user_id=" + std::to_string(uid) + " AND status=1";

    mysql_query(m, q.c_str());
    MYSQL_RES* res = mysql_store_result(m);

    if (!res) { 
        mysql_pool_->release(m); 
        return ; 
    }

    MYSQL_ROW row;

    while ((row = mysql_fetch_row(res))) {

        int64_t fid = std::stoll(row[0]);

        Buffer body;
        body.write_int64(uid);
        body.write_int8(status);

        std::string packet = MessageHelper::pack_header(MsgType::FRIEND_STATUS_NOTIFY, body.data().size(), 0);
        packet.append(body.data());

        send_to_user(fid, packet);
    }

    mysql_free_result(res);
    mysql_pool_->release(m);

}
void Server::handle_friend_add(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {
    int64_t uid = conn->get_user_id();

    if (!conn->is_authenticated()) {
        return;
    }

    std::string target_name = body.read_string();

    uint32_t seq = hdr.sequence;

    worker_pool_->submit([this, conn, uid, target_name, seq]() {
        auto* m = mysql_pool_->acquire();
        
        // 查找目标用户
        std::string q = "SELECT id FROM users WHERE username='" + esc(m, target_name) + "'";

        mysql_query(m, q.c_str());

        MYSQL_RES* res = mysql_store_result(m);

        if (!res) { 
            mysql_pool_->release(m); 
            return ; 
        }

        MYSQL_ROW row = mysql_fetch_row(res);
        
        if (!row) {

            mysql_free_result(res);
            mysql_pool_->release(m);

            Buffer resp;
            resp.write_int8(0);
            resp.write_string("用户不存在");

            std::string packet = MessageHelper::pack_header(MsgType::FRIEND_ADD_RESPONSE, resp.data().size(), seq);
            packet.append(resp.data());
            
            conn->send_raw(packet);

            return;
        }
        
        int64_t fid = std::stoll(row[0]);
        mysql_free_result(res);
        
        if (uid == fid) {
            mysql_pool_->release(m);

            Buffer resp;
            resp.write_int8(0);
            resp.write_string("不能添加自己");

            std::string packet = MessageHelper::pack_header(MsgType::FRIEND_ADD_RESPONSE, resp.data().size(), seq);
            packet.append(resp.data());
            
            conn->send_raw(packet);

            return;
        }

        if (is_friend(uid, fid)) {
            mysql_pool_->release(m);

            Buffer resp;
            resp.write_int8(0);
            resp.write_string("已经是好友");

            std::string packet = MessageHelper::pack_header(MsgType::FRIEND_ADD_RESPONSE, resp.data().size(), seq);
            packet.append(resp.data());
            
            conn->send_raw(packet);

            return;
        }

        std::string requester_name = conn->get_user_info().username;

        Buffer notify;
        notify.write_int64(uid);
        notify.write_string(requester_name);

        std::string np = MessageHelper::pack_header(MsgType::FRIEND_ADD_REQUEST, notify.data().size(), 0);
        np.append(notify.data());

        send_to_user(fid, np);

        // 双向插入好友关系（事务保证原子性，避免单边好友）
        mysql_query(m, "START TRANSACTION");

        q = "INSERT INTO friendships (user_id, friend_id, status, created_at) VALUES ("
            + std::to_string(uid) + "," + std::to_string(fid) + ",1," + std::to_string(time(nullptr)) + ")";

        mysql_query(m, q.c_str());

        q = "INSERT INTO friendships (user_id, friend_id, status, created_at) VALUES ("
            + std::to_string(fid) + "," + std::to_string(uid) + ",1," + std::to_string(time(nullptr)) + ")";

        mysql_query(m, q.c_str());

        if (mysql_errno(m) == 0) {
            mysql_query(m, "COMMIT");
        } else {
            LOG_ERROR("friend add failed: {}", mysql_error(m));
            mysql_query(m, "ROLLBACK");
        }
        mysql_pool_->release(m);

        Buffer resp;
        resp.write_int8(1);
        resp.write_string("好友添加成功");

        std::string packet = MessageHelper::pack_header(MsgType::FRIEND_ADD_RESPONSE, resp.data().size(), seq);
        packet.append(resp.data());

        conn->send_raw(packet);

    });
}
void Server::handle_friend_delete(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {

    int64_t uid = conn->get_user_id();

    if (!conn->is_authenticated()) {
        return;
    }

    int64_t fid = body.read_int64();
    uint32_t seq = hdr.sequence;

    worker_pool_->submit([this, conn, uid, fid, seq]() {
        auto* m = mysql_pool_->acquire();

        std::string q = "DELETE FROM friendships WHERE (user_id=" + std::to_string(uid) 
                        + " AND friend_id=" + std::to_string(fid) + ") OR (user_id=" 
                        + std::to_string(fid) + " AND friend_id=" + std::to_string(uid) + ")";

        mysql_query(m, q.c_str());
        mysql_pool_->release(m);

        Buffer resp;
        resp.write_int8(1);
        resp.write_string("好友已删除");

        std::string packet = MessageHelper::pack_header(MsgType::FRIEND_DELETE_RESPONSE, resp.data().size(), seq);
        packet.append(resp.data());

        conn->send_raw(packet);
    });
}
void Server::handle_friend_list(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {
    int64_t uid = conn->get_user_id();

    if (!conn->is_authenticated()) {
        return;
    }

    uint32_t seq = hdr.sequence;

    worker_pool_->submit([this, conn, uid, seq]() {

        auto* m = mysql_pool_->acquire();

        std::string q = "SELECT u.id, u.username, u.nickname, u.status, "
                        "CASE WHEN f.blocked_by=" + std::to_string(uid) + " THEN 1 ELSE 0 END "
                        "FROM users u JOIN friendships f ON u.id=f.friend_id "
                        "WHERE f.user_id=" + std::to_string(uid) + " AND f.status IN (1,3)";

        mysql_query(m, q.c_str());
        MYSQL_RES* res = mysql_store_result(m);

        if (!res) { 
            mysql_pool_->release(m); 
            return ; 
        }

        int count = mysql_num_rows(res);
        
        Buffer resp;
        resp.write_int32(count);

        MYSQL_ROW row;

        while ((row = mysql_fetch_row(res))) {
            resp.write_int64(std::stoll(row[0]));  // id
            resp.write_string(row[1] ? row[1] : ""); // username
            resp.write_string(row[2] ? row[2] : ""); // nickname
            resp.write_int8(std::stoi(row[3]));     // status
            resp.write_int8(std::stoi(row[4]));     // blocked
        }

        mysql_free_result(res);
        mysql_pool_->release(m);

        std::string packet = MessageHelper::pack_header(MsgType::FRIEND_LIST_RESPONSE, resp.data().size(), seq);
        packet.append(resp.data());
        
        conn->send_raw(packet);

    });
}
void Server::handle_friend_block(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {
    int64_t uid = conn->get_user_id();

    if (!conn->is_authenticated()) {
        return;
    }

    int64_t fid = body.read_int64();
    uint32_t seq = hdr.sequence;

    worker_pool_->submit([this, conn, uid, fid, seq]() {
        auto* m = mysql_pool_->acquire();
        std::string q = "UPDATE friendships SET status=3, blocked_by=" + std::to_string(uid)
                        + " WHERE user_id=" + std::to_string(uid) + " AND friend_id=" + std::to_string(fid);
        mysql_query(m, q.c_str());
        mysql_pool_->release(m);

        Buffer resp;
        resp.write_int8(1);
        resp.write_string("已屏蔽");

        std::string packet = MessageHelper::pack_header(MsgType::FRIEND_BLOCK_RESPONSE, resp.data().size(), seq);
        packet.append(resp.data());

        conn->send_raw(packet);
    });
}

void Server::handle_friend_unblock(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {
    int64_t uid = conn->get_user_id();

    if (!conn->is_authenticated()) {
        return;
    }

    int64_t fid = body.read_int64();
    uint32_t seq = hdr.sequence;

    worker_pool_->submit([this, conn, uid, fid, seq]() {
        auto* m = mysql_pool_->acquire();

        std::string q = "UPDATE friendships SET status=1, blocked_by=NULL "
                        "WHERE user_id=" + std::to_string(uid) + " AND friend_id=" + std::to_string(fid);

        mysql_query(m, q.c_str());
        mysql_pool_->release(m);

        Buffer resp; resp.write_int8(1); resp.write_string("已解除屏蔽");

        std::string pkt = MessageHelper::pack_header(MsgType::FRIEND_UNBLOCK_RESPONSE, resp.data().size(), seq);
        pkt.append(resp.data()); conn->send_raw(pkt);
    });
}

void Server::handle_private_message(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {
    int64_t uid = conn->get_user_id();

    if (!conn->is_authenticated()) {
        return;
    }
    int64_t to_id = body.read_int64();
    std::string content = body.read_string();

    uint32_t seq = hdr.sequence;

    worker_pool_->submit([this, conn, uid, to_id, content, seq]() {
        // 检查是否为好友
        if (!is_friend(uid, to_id)) {
            Buffer resp;
            resp.write_int8(0);
            resp.write_string("不是好友，无法发送消息");

            std::string packet = MessageHelper::pack_header(MsgType::PRIVATE_MESSAGE_ACK, resp.data().size(), seq);
            
            packet.append(resp.data());
            conn->send_raw(packet);
            
            return;
        }

        // 检查是否被屏蔽
        if (is_blocked(uid, to_id)) {
            Buffer resp;
            resp.write_int8(0);
            resp.write_string("消息已被对方屏蔽");

            std::string packet = MessageHelper::pack_header(MsgType::PRIVATE_MESSAGE_ACK, resp.data().size(), seq);
            packet.append(resp.data());
            
            conn->send_raw(packet);
            
            return;
        }

        // 保存消息
        std::string msg_id = Crypto::generate_transfer_id();

        auto* m = mysql_pool_->acquire();

        std::string q = "INSERT INTO private_messages (message_id, sender_id, receiver_id, content, message_type, sent_at) VALUES ('"
                        + msg_id + "'," + std::to_string(uid) + "," + std::to_string(to_id) + ",'"
                        + esc(m, content) + "',1," + std::to_string(time(nullptr)) + ")";

        mysql_query(m, q.c_str());
        mysql_pool_->release(m);

        // 转发给接收者
        Buffer fwd;
        fwd.write_string(msg_id);
        fwd.write_int64(uid);
        fwd.write_string(conn->get_user_info().username);
        fwd.write_string(content);
        fwd.write_int64(time(nullptr));

        std::string packet = MessageHelper::pack_header(MsgType::PRIVATE_MESSAGE, fwd.data().size(), 0);
        packet.append(fwd.data());

        send_to_user(to_id, packet);

        // 回复发送者
        Buffer ack;
        ack.write_int8(1);
        ack.write_string(msg_id);

        std::string ack_packet = MessageHelper::pack_header(MsgType::PRIVATE_MESSAGE_ACK, ack.data().size(), seq);
        ack_packet.append(ack.data());

        conn->send_raw(ack_packet);

    });
}

void Server::handle_private_history(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {
    int64_t uid = conn->get_user_id();

    if (!conn->is_authenticated()) {
        return;
    }

    int64_t fid = body.read_int64();
    int64_t before_ts = body.end() ? 0 : body.read_int64();
    uint32_t seq = hdr.sequence;

    worker_pool_->submit([this, conn, uid, fid, before_ts, seq]() {

        auto* m = mysql_pool_->acquire();

        std::string time_filter = before_ts > 0 ? " AND sent_at < " + std::to_string(before_ts) : "";
        std::string ftime_filter = before_ts > 0 ? " AND created_at < " + std::to_string(before_ts) : "";

        std::string q = "(SELECT message_id, sender_id, content, message_type, sent_at "
                        "FROM private_messages WHERE (sender_id=" + std::to_string(uid)
                        + " AND receiver_id=" + std::to_string(fid) + ") OR (sender_id="
                        + std::to_string(fid) + " AND receiver_id=" + std::to_string(uid) + ")" + time_filter + ") "
                        "UNION ALL "
                        "(SELECT transfer_id, sender_id, file_name, 2, created_at "
                        "FROM file_transfers WHERE ((sender_id=" + std::to_string(uid)
                        + " AND receiver_id=" + std::to_string(fid) + ") OR (sender_id="
                        + std::to_string(fid) + " AND receiver_id=" + std::to_string(uid)
                        + ")) AND group_id=0 AND status=1" + ftime_filter + ") "
                        "ORDER BY 5 DESC LIMIT 500";

        if (mysql_query(m, q.c_str()) != 0) {
            mysql_pool_->release(m);
            return;
        }

        MYSQL_RES* res = mysql_store_result(m);

        if (!res) {
            mysql_pool_->release(m);
            return;
        }

        int count = mysql_num_rows(res);

        Buffer resp;
        resp.write_int32(count);

        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res))) {

            resp.write_string(row[0] ? row[0] : "");
            resp.write_int64(row[1] ? std::stoll(row[1]) : 0);
            resp.write_string(row[2] ? row[2] : "");
            resp.write_int8(row[3] ? std::stoi(row[3]) : 1);
            resp.write_int64(row[4] ? std::stoll(row[4]) : 0);

        }

        mysql_free_result(res);
        mysql_pool_->release(m);

        std::string packet = MessageHelper::pack_header(MsgType::PRIVATE_HISTORY_RESPONSE, resp.data().size(), seq);
        packet.append(resp.data());

        conn->send_raw(packet);
    });
}

// 文件传输 

void Server::handle_file_transfer_init(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {
    if (!conn->is_authenticated()) {
        return;
    }
    int64_t uid = conn->get_user_id();
    int64_t to_id = body.read_int64();
    int64_t gid = body.read_int64();

    std::string fname = body.read_string();

    int64_t fsize = body.read_int64();
    uint32_t seq = hdr.sequence;

    worker_pool_->submit([this, conn, uid, to_id, gid, fname, fsize, seq]() {
        std::string tid = Crypto::generate_transfer_id();
        auto* m = mysql_pool_->acquire();

        // 在磁盘上创建文件
        std::string file_path = "../uploads/" + tid;
        int fd = open(file_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            LOG_ERROR("File create failed: {} err={}", file_path, strerror(errno));
            mysql_pool_->release(m);
            Buffer resp;
            resp.write_string("");
            std::string pkt = MessageHelper::pack_header(MsgType::FILE_TRANSFER_INIT_ACK, resp.data().size(), seq);
            pkt.append(resp.data()); conn->send_raw(pkt);
            return;
        }
        {
            std::lock_guard<std::mutex> lock(file_fds_mutex_);
            file_fds_[tid] = fd;
        }

        std::string escaped_fname = esc(m, fname);

        std::string q = "INSERT INTO file_transfers (transfer_id, sender_id, receiver_id, group_id, file_name, file_size, file_path, created_at) VALUES ('"
        + tid + "'," + std::to_string(uid) + "," + std::to_string(to_id) + "," + std::to_string(gid) + ",'"
        + escaped_fname + "'," + std::to_string(fsize) + ",'" + esc(m, file_path) + "'," + std::to_string(time(nullptr)) + ")";

        if (mysql_query(m, q.c_str()) != 0) {
            LOG_ERROR("File transfer insert failed: {}", mysql_error(m));
        }
        mysql_pool_->release(m);

        Buffer resp;
        resp.write_string(tid);
        std::string pkt = MessageHelper::pack_header(MsgType::FILE_TRANSFER_INIT_ACK, resp.data().size(), seq);
        pkt.append(resp.data()); conn->send_raw(pkt);

        LOG_INFO("File transfer init tid={} path={} size={} gid={}", tid, file_path, fsize, gid);
    });
}

void Server::handle_file_transfer_chunk(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {
    if (!conn->is_authenticated()) {
        return;
    }
    std::string tid = body.read_string();
    std::string data = body.read_string();

    // 复用 INIT 阶段打开的文件句柄，避免每片 open/close
    int fd = -1;
    {
        std::lock_guard<std::mutex> lock(file_fds_mutex_);
        auto it = file_fds_.find(tid);
        if (it == file_fds_.end()) {
            LOG_ERROR("Chunk write failed: unknown tid={}", tid);
            return;
        }
        fd = it->second;
    }

    ssize_t written = write(fd, data.data(), data.size());
    if (written != (ssize_t)data.size()) {
        LOG_ERROR("Chunk write incomplete: {} / {}", written, data.size());
    }
}

void Server::handle_file_transfer_complete(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {
    if (!conn->is_authenticated()) {
        return;
    }
    std::string tid = body.read_string();

    // 关闭并释放该传输的文件句柄
    {
        std::lock_guard<std::mutex> lock(file_fds_mutex_);
        auto it = file_fds_.find(tid);
        if (it != file_fds_.end()) {
            close(it->second);
            file_fds_.erase(it);
        }
    }

    // 查询传输信息并标记完成
    auto* m = mysql_pool_->acquire();
    std::string q = "SELECT sender_id, receiver_id, group_id, file_name, file_size FROM file_transfers WHERE transfer_id='" + esc(m, tid) + "'";
    mysql_query(m, q.c_str());
    MYSQL_RES* res = mysql_store_result(m);

    if (!res || mysql_num_rows(res) == 0) {
        if (res) mysql_free_result(res);
        mysql_pool_->release(m);
        return;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    int64_t sender_id = row[0] ? std::stoll(row[0]) : 0;
    int64_t receiver_id = row[1] ? std::stoll(row[1]) : 0;
    int64_t group_id = row[2] ? std::stoll(row[2]) : 0;
    std::string fname = row[3] ? row[3] : "";
    int64_t fsize = row[4] ? std::stoll(row[4]) : 0;
    mysql_free_result(res);

    // 标记完成
    mysql_query(m, ("UPDATE file_transfers SET status=1 WHERE transfer_id='" + esc(m, tid) + "'").c_str());
    mysql_pool_->release(m);

    // 文件上传完毕，现在通知接收方
    Buffer notif;
    notif.write_string(tid);
    notif.write_int64(sender_id);
    notif.write_string(fname);
    notif.write_int64(fsize);
    if (group_id > 0) notif.write_int64(group_id);

    std::string np = MessageHelper::pack_header(MsgType::FILE_TRANSFER_INIT, notif.data().size(), 0);
    np.append(notif.data());

    if (group_id > 0) {
        send_to_group(group_id, sender_id, np);
    } else {
        send_to_user(receiver_id, np);
    }
    LOG_INFO("File transfer complete tid={} size={} gid={}", tid, fsize, group_id);
}

void Server::send_offline_files(int64_t uid) {
    auto* m = mysql_pool_->acquire();

    std::string q = "SELECT transfer_id, sender_id, file_name, file_size FROM file_transfers "
                    "WHERE receiver_id=" + std::to_string(uid) + " AND status=1 AND notified=0";

    if (mysql_query(m, q.c_str()) != 0) { 
        mysql_pool_->release(m); 
        return; 
    }

    MYSQL_RES* res = mysql_store_result(m);

    if (!res) { 
        mysql_pool_->release(m); 
        return; 
    }

    MYSQL_ROW row;

    while ((row = mysql_fetch_row(res))) {
        Buffer body;
        body.write_string(row[0] ? row[0] : "");
        body.write_int64(row[1] ? std::stoll(row[1]) : 0);
        body.write_string(row[2] ? row[2] : "");
        body.write_int64(row[3] ? std::stoll(row[3]) : 0);

        std::string pkt = MessageHelper::pack_header(MsgType::FILE_TRANSFER_INIT, body.data().size(), 0);
        pkt.append(body.data()); 
        send_to_user(uid, pkt);

    }

    mysql_free_result(res);
    LOG_INFO("send_offline_files uid={} count={}", uid, mysql_num_rows(res));

    mysql_query(m, ("UPDATE file_transfers SET notified=1 WHERE receiver_id=" + std::to_string(uid) + " AND status=1 AND notified=0").c_str());
    mysql_pool_->release(m);
}

void Server::handle_file_download(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {
    if (!conn->is_authenticated()) {
        return;
    }
    std::string tid = body.read_string();
    uint32_t seq = hdr.sequence;

    auto* m = mysql_pool_->acquire();
    std::string q = "SELECT file_name, file_size, file_path FROM file_transfers WHERE transfer_id='" + esc(m, tid) + "'";

    mysql_query(m, q.c_str());
    MYSQL_RES* res = mysql_store_result(m);

    if (!res) {
        LOG_ERROR("File download query failed: {} tid={}", mysql_error(m), tid);
        mysql_pool_->release(m);
        send_error(conn, seq, "server error");
        return;
    }

    if (MYSQL_ROW row = mysql_fetch_row(res)) {
        std::string fname = row[0] ? row[0] : "";
        uint64_t fsize = row[1] ? std::stoull(row[1]) : 0;
        std::string file_path = row[2] ? row[2] : "";

        mysql_free_result(res);
        mysql_pool_->release(m);

        // 检查磁盘文件是否存在
        struct stat st;
        if (stat(file_path.c_str(), &st) != 0) {
            LOG_ERROR("File not found on disk: {}", file_path);
            send_error(conn, seq, "file not found on disk");
            return;
        }
        fsize = st.st_size;

        LOG_INFO("File download tid={} name={} path={} size={}", tid, fname, file_path, fsize);

        // 读取文件并构建完整响应
        int fd = open(file_path.c_str(), O_RDONLY);
        if (fd < 0) {
            LOG_ERROR("File open failed: {}", file_path);
            send_error(conn, seq, "file read error");
            return;
        }
        std::string file_data(fsize, 0);
        ssize_t n = read(fd, &file_data[0], fsize);
        close(fd);
        if (n != (ssize_t)fsize) {
            LOG_ERROR("File read incomplete: {} / {}", n, fsize);
            send_error(conn, seq, "file read error");
            return;
        }

        Buffer resp;
        resp.write_string(tid);
        resp.write_string(fname);
        resp.write_int64(fsize);
        resp.write_string(file_data);

        std::string pkt = MessageHelper::pack_header(MsgType::FILE_DOWNLOAD_RESPONSE, resp.data().size(), seq);
        pkt.append(resp.data());
        conn->send_raw(pkt);
        LOG_INFO("File download sent tid={} size={}", tid, fsize);
    } else {
        mysql_free_result(res);
        mysql_pool_->release(m);
        send_error(conn, seq, "file not found");
    }
}


//  群组辅助 

std::string Server::get_username(int64_t uid) {
    auto* m = mysql_pool_->acquire();

    std::string q = "SELECT username FROM users WHERE id=" + std::to_string(uid);

    mysql_query(m, q.c_str());
    MYSQL_RES* res = mysql_store_result(m);

    if (!res) { 
        mysql_pool_->release(m); 
        return ""; 
    }

    std::string name;

    if (MYSQL_ROW row = mysql_fetch_row(res)) {
        name = row[0] ? row[0] : "";
    }

    mysql_free_result(res);
    mysql_pool_->release(m);

    return name;

}

bool Server::is_group_member(int64_t gid, int64_t uid) {
    auto* m = mysql_pool_->acquire();
    std::string q = "SELECT COUNT(*) FROM group_members WHERE group_id="
                    + std::to_string(gid) + " AND user_id=" + std::to_string(uid);

    mysql_query(m, q.c_str());
    MYSQL_RES* res = mysql_store_result(m);

    if (!res) { 
        mysql_pool_->release(m); 
        return false; 
    }

    int count = 0;

    if (MYSQL_ROW row = mysql_fetch_row(res)) {
        count = std::stoi(row[0]);
    }

    mysql_free_result(res);
    mysql_pool_->release(m);

    return count > 0;
}

int Server::get_group_role(int64_t gid, int64_t uid) {
    auto* m = mysql_pool_->acquire();

    std::string q = "SELECT role FROM group_members WHERE group_id="
                    + std::to_string(gid) + " AND user_id=" + std::to_string(uid);

    mysql_query(m, q.c_str());
    MYSQL_RES* res = mysql_store_result(m);

    if (!res) { 
        mysql_pool_->release(m); 
        return -1; 
    }

    int role = -1;

    if (MYSQL_ROW row = mysql_fetch_row(res)) {
        role = row[0] ? std::stoi(row[0]) : -1;
    }

    mysql_free_result(res);
    mysql_pool_->release(m);

    return role;
}

std::vector<int64_t> Server::get_group_member_ids(int64_t gid) {
    std::vector<int64_t> ids;

    auto* m = mysql_pool_->acquire();

    std::string q = "SELECT user_id FROM group_members WHERE group_id=" + std::to_string(gid);

    mysql_query(m, q.c_str());
    MYSQL_RES* res = mysql_store_result(m);

    if (!res) { 
        mysql_pool_->release(m); 
        return {}; }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        ids.push_back(std::stoll(row[0]));
    }

    mysql_free_result(res);
    mysql_pool_->release(m);

    return ids;
}

void Server::send_to_group(int64_t gid, int64_t exclude_uid, const std::string& data) {
    auto ids = get_group_member_ids(gid);

    for (int64_t uid : ids) {

        if (uid == exclude_uid) {
            continue;
        }

        send_to_user(uid, data);
    }

}

void Server::send_group_notify(int64_t gid, const std::string& msg) {
    Buffer body;
    body.write_int64(gid);
    body.write_string(msg);

    std::string pkt = MessageHelper::pack_header(MsgType::GROUP_NOTIFY, body.data().size(), 0);
    pkt.append(body.data());

    send_to_group(gid, 0, pkt);
}

// 创建群 

void Server::handle_group_create(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {
    int64_t uid = conn->get_user_id();

    if (!conn->is_authenticated()) {
        return;
    }

    std::string group_name = body.read_string();
    std::string desc = body.read_string();

    uint32_t seq = hdr.sequence;

    worker_pool_->submit([this, conn, uid, group_name, desc, seq]() {
        auto* m = mysql_pool_->acquire();
        std::string q = "SELECT COUNT(*) FROM groups_info WHERE group_name='" + esc(m, group_name) + "'";

        mysql_query(m, q.c_str());
        MYSQL_RES* res = mysql_store_result(m);

        if (!res) { 
            mysql_pool_->release(m); 
            return ; 
        }

        MYSQL_ROW row = mysql_fetch_row(res);

        if (row && std::stoi(row[0]) > 0) {
            mysql_free_result(res); 
            mysql_pool_->release(m);

            Buffer resp; resp.write_int8(0); 
            resp.write_string("群名已存在");

            std::string pkt = MessageHelper::pack_header(MsgType::GROUP_CREATE_RESPONSE, resp.data().size(), seq);
            pkt.append(resp.data()); 
            
            conn->send_raw(pkt); 
            return;

        }

        mysql_free_result(res);

        q = "INSERT INTO groups_info (group_name, owner_id, description, created_at) VALUES ('"
            + esc(m, group_name) + "'," + std::to_string(uid) + ",'" + esc(m, desc) + "',"
            + std::to_string(time(nullptr)) + ")";

        mysql_query(m, q.c_str());
        int64_t gid = mysql_insert_id(m);

        q = "INSERT INTO group_members (group_id, user_id, role, joined_at) VALUES ("
            + std::to_string(gid) + "," + std::to_string(uid) + ",2,"
            + std::to_string(time(nullptr)) + ")";

        mysql_query(m, q.c_str());
        mysql_pool_->release(m);

        LOG_INFO("Group '{}' ({}) created by {}", group_name, gid, uid);

        Buffer resp; 
        resp.write_int8(1); 
        resp.write_int64(gid); 
        resp.write_string(group_name);

        std::string pkt = MessageHelper::pack_header(MsgType::GROUP_CREATE_RESPONSE, resp.data().size(), seq);

        pkt.append(resp.data()); 
        conn->send_raw(pkt);

    });

}

// 解散群 

void Server::handle_group_dissolve(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {
    int64_t uid = conn->get_user_id();

    if (!conn->is_authenticated()) {
        return;
    }

    int64_t gid = body.read_int64();
    uint32_t seq = hdr.sequence;

    worker_pool_->submit([this, conn, uid, gid, seq]() {

        if (get_group_role(gid, uid) != 2) {
            Buffer resp; 
            resp.write_int8(0); 
            resp.write_string("仅群主可解散");

            std::string pkt = MessageHelper::pack_header(MsgType::GROUP_DISSOLVE_RESPONSE, resp.data().size(), seq);
            pkt.append(resp.data()); 
            
            conn->send_raw(pkt); 
            return;
        }

        send_group_notify(gid, "群已被解散");
        auto* m = mysql_pool_->acquire();

        mysql_query(m, ("DELETE FROM groups_info WHERE id=" + std::to_string(gid)).c_str());
        mysql_pool_->release(m);

        LOG_INFO("Group {} dissolved by {}", gid, uid);

        Buffer resp; 
        resp.write_int8(1); 
        resp.write_string("群已解散");

        std::string pkt = MessageHelper::pack_header(MsgType::GROUP_DISSOLVE_RESPONSE, resp.data().size(), seq);
        pkt.append(resp.data()); 
        
        conn->send_raw(pkt);
    });
}

// ======================== 申请加群 ========================

void Server::handle_group_join(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {
    int64_t uid = conn->get_user_id();
    if (!conn->is_authenticated()) {
        return;
    }

    int64_t gid = body.read_int64();
    std::string msg = body.read_string();

    uint32_t seq = hdr.sequence;

    worker_pool_->submit([this, conn, uid, gid, msg, seq]() {
        auto* m = mysql_pool_->acquire();

        std::string check_q = "SELECT COUNT(*) FROM groups_info WHERE id=" + std::to_string(gid);
        mysql_query(m, check_q.c_str());

        MYSQL_RES* check_res = mysql_store_result(m);

        if (!check_res) { 
            mysql_pool_->release(m); 
            return; 
        }

        MYSQL_ROW check_row = mysql_fetch_row(check_res);

        if (!check_row || std::stoi(check_row[0]) == 0) {

            mysql_free_result(check_res);
            mysql_pool_->release(m);

            Buffer resp; resp.write_int8(0); 
            resp.write_string("群不存在");

            std::string pkt = MessageHelper::pack_header(MsgType::GROUP_JOIN_RESPONSE, resp.data().size(), seq);
            pkt.append(resp.data()); 
            
            conn->send_raw(pkt); 
            return;
        }
        mysql_free_result(check_res);

        if (is_group_member(gid, uid)) {

            mysql_pool_->release(m);

            Buffer resp; 
            resp.write_int8(0); 
            resp.write_string("已在群中");

            std::string pkt = MessageHelper::pack_header(MsgType::GROUP_JOIN_RESPONSE, resp.data().size(), seq);
            pkt.append(resp.data()); 
            
            conn->send_raw(pkt); 
            return;
        }

        // 检查是否已有待处理的申请，避免重复提交
        std::string pending_q = "SELECT COUNT(*) FROM group_join_requests WHERE group_id="
                                + std::to_string(gid) + " AND user_id=" + std::to_string(uid)
                                + " AND status=0";
        mysql_query(m, pending_q.c_str());
        MYSQL_RES* pres = mysql_store_result(m);
        MYSQL_ROW prow = pres ? mysql_fetch_row(pres) : nullptr;

        if (prow && std::stoi(prow[0]) > 0) {
            if (pres) mysql_free_result(pres);
            mysql_pool_->release(m);

            Buffer resp; 
            resp.write_int8(0); 
            resp.write_string("已有待处理的申请，请等待审批");

            std::string pkt = MessageHelper::pack_header(MsgType::GROUP_JOIN_RESPONSE, resp.data().size(), seq);
            pkt.append(resp.data()); 
            
            conn->send_raw(pkt); 
            return;
        }
        if (pres) mysql_free_result(pres);

        std::string q = "INSERT INTO group_join_requests (group_id, user_id, status, message, created_at) VALUES ("
                        + std::to_string(gid) + "," + std::to_string(uid) + ",0,'" + esc(m, msg) + "',"
                        + std::to_string(time(nullptr)) + ")";

        mysql_query(m, q.c_str());

        int64_t req_id = mysql_insert_id(m);

        auto ids = get_group_member_ids(gid);

        std::string uname = get_username(uid);

        for (int64_t mid : ids) {

            if (get_group_role(gid, mid) >= 1) {
                Buffer n; 
                n.write_int64(gid); 
                n.write_int64(req_id); 
                n.write_int64(uid);
                n.write_string(uname); 
                n.write_string(msg);

                std::string np = MessageHelper::pack_header(MsgType::GROUP_JOIN_REQ_NOTIFY, n.data().size(), 0);
                np.append(n.data()); 

                send_to_user(mid, np);
            }
        }

        mysql_pool_->release(m);

        Buffer resp; 
        resp.write_int8(1); 
        resp.write_string("申请已发送");

        std::string pkt = MessageHelper::pack_header(MsgType::GROUP_JOIN_RESPONSE, resp.data().size(), seq);
        pkt.append(resp.data()); 
        
        conn->send_raw(pkt);
    });
}

// ======================== 管理员处理加群申请 ========================

void Server::handle_group_join_req_handle(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {
    int64_t uid = conn->get_user_id();

    if (!conn->is_authenticated()) {
        return;
    }

    int64_t req_id = body.read_int64();
    uint8_t approve = body.read_int8();
    uint32_t seq = hdr.sequence;

    worker_pool_->submit([this, conn, uid, req_id, approve, seq]() {

        auto* m = mysql_pool_->acquire();
        std::string q = "SELECT group_id, user_id FROM group_join_requests WHERE id="
                        + std::to_string(req_id) + " AND status=0";

        mysql_query(m, q.c_str());

        MYSQL_RES* res = mysql_store_result(m);

        if (!res || mysql_num_rows(res) == 0) {

            mysql_free_result(res); 
            mysql_pool_->release(m);

            Buffer resp; 
            resp.write_int8(0); 
            resp.write_string("无效申请");

            std::string pkt = MessageHelper::pack_header(MsgType::GROUP_JOIN_REQ_HANDLE_ACK, resp.data().size(), seq);
            pkt.append(resp.data()); 
            
            conn->send_raw(pkt); 
            return;
        }

        MYSQL_ROW row = mysql_fetch_row(res);

        int64_t gid = std::stoll(row[0]);
        int64_t req_uid = std::stoll(row[1]);

        mysql_free_result(res);

        if (get_group_role(gid, uid) < 1) {
            mysql_pool_->release(m);
            Buffer resp; resp.write_int8(0); resp.write_string("权限不足");

            std::string pkt = MessageHelper::pack_header(MsgType::GROUP_JOIN_REQ_HANDLE_ACK, resp.data().size(), seq);
            pkt.append(resp.data()); 
            
            conn->send_raw(pkt); 
            return;
        }

        if (approve) {
            q = "INSERT INTO group_members (group_id, user_id, role, joined_at) VALUES ("
                + std::to_string(gid) + "," + std::to_string(req_uid) + ",0,"
                + std::to_string(time(nullptr)) + ")";

            mysql_query(m, q.c_str());

            q = "UPDATE group_join_requests SET status=1, handled_at=" + std::to_string(time(nullptr))
                + " WHERE id=" + std::to_string(req_id);

            mysql_query(m, q.c_str());

            Buffer n; 
            n.write_int64(gid);
            n.write_string("你已加入群");
            std::string np = MessageHelper::pack_header(MsgType::GROUP_NOTIFY, n.data().size(), 0);
            np.append(n.data()); 
            
            send_to_user(req_uid, np);

        } 
        else {
            q = "UPDATE group_join_requests SET status=2, handled_at=" + std::to_string(time(nullptr))
                + " WHERE id=" + std::to_string(req_id);

            mysql_query(m, q.c_str());

            Buffer n; 
            n.write_int64(gid); 
            n.write_string("你的加群申请已被拒绝");
            std::string np = MessageHelper::pack_header(MsgType::GROUP_NOTIFY, n.data().size(), 0);
            np.append(n.data()); 

            send_to_user(req_uid, np);
        }

        mysql_pool_->release(m);

        Buffer resp; 
        resp.write_int8(1); 
        resp.write_string(approve ? "已同意" : "已拒绝");

        std::string pkt = MessageHelper::pack_header(MsgType::GROUP_JOIN_REQ_HANDLE_ACK, resp.data().size(), seq);

        pkt.append(resp.data()); 
        
        conn->send_raw(pkt);

    });
}

// ======================== 退出群 ========================

void Server::handle_group_quit(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {
    int64_t uid = conn->get_user_id();

    if (!conn->is_authenticated()) {
        return;
    }

    int64_t gid = body.read_int64();
    uint32_t seq = hdr.sequence;

    worker_pool_->submit([this, conn, uid, gid, seq]() {

        if (!is_group_member(gid, uid)) {

            Buffer resp; 
            resp.write_int8(0); 
            resp.write_string("不在群中");
            std::string pkt = MessageHelper::pack_header(MsgType::GROUP_QUIT_RESPONSE, resp.data().size(), seq);
            pkt.append(resp.data()); 
            
            conn->send_raw(pkt); 
            return;

        }

        auto* m = mysql_pool_->acquire();
        mysql_query(m, ("DELETE FROM group_members WHERE group_id=" + std::to_string(gid)
                        + " AND user_id=" + std::to_string(uid)).c_str());

        mysql_pool_->release(m);

        // 通知群里其他成员
        send_group_notify(gid, get_username(uid) + " 已退出群");

        LOG_INFO("User {} quit group {}", uid, gid);

        Buffer resp; 
        resp.write_int8(1); 
        resp.write_string("已退出群");

        std::string pkt = MessageHelper::pack_header(MsgType::GROUP_QUIT_RESPONSE, resp.data().size(), seq);
        pkt.append(resp.data()); 
        
        conn->send_raw(pkt);
    });
}

// ======================== 踢人 ========================

void Server::handle_group_kick(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {
    int64_t uid = conn->get_user_id();

    if (!conn->is_authenticated()) {
        return;
    }

    int64_t gid = body.read_int64();
    int64_t target_uid = body.read_int64();
    uint32_t seq = hdr.sequence;

    worker_pool_->submit([this, conn, uid, gid, target_uid, seq]() {

        if (get_group_role(gid, uid) < 1) {

            Buffer resp; 
            resp.write_int8(0); 
            resp.write_string("权限不足");

            std::string pkt = MessageHelper::pack_header(MsgType::GROUP_KICK_RESPONSE, resp.data().size(), seq);
            pkt.append(resp.data()); 
            
            conn->send_raw(pkt); 
            return;

        }
        if (get_group_role(gid, target_uid) >= get_group_role(gid, uid)) {
            Buffer resp; 
            resp.write_int8(0);
            resp.write_string("无法踢出同等级或更高权限成员");
            std::string pkt = MessageHelper::pack_header(MsgType::GROUP_KICK_RESPONSE, resp.data().size(), seq);
            pkt.append(resp.data()); 
            
            conn->send_raw(pkt); 
            return;

        }

        auto* m = mysql_pool_->acquire();

        mysql_query(m, ("DELETE FROM group_members WHERE group_id=" + std::to_string(gid)
                        + " AND user_id=" + std::to_string(target_uid)).c_str());

        mysql_pool_->release(m);

        // 通知群里其他成员
        send_group_notify(gid, get_username(target_uid) + " 已被移出群");

        Buffer n; 
        n.write_int64(gid); 
        n.write_string("你已被移出群");

        std::string np = MessageHelper::pack_header(MsgType::GROUP_NOTIFY, n.data().size(), 0);
        np.append(n.data()); 
        
        send_to_user(target_uid, np);

        Buffer resp; 
        resp.write_int8(1); 
        resp.write_string("已踢出");

        std::string pkt = MessageHelper::pack_header(MsgType::GROUP_KICK_RESPONSE, resp.data().size(), seq);
        pkt.append(resp.data()); 
        
        conn->send_raw(pkt);
    });
}

void Server::handle_group_set_admin(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {
    int64_t uid = conn->get_user_id();

    if (!conn->is_authenticated()) {
        return;
    }

    int64_t gid = body.read_int64();
    int64_t target_uid = body.read_int64();
    uint8_t set_admin = body.read_int8();  // 1=设为管理员, 0=取消管理员
    uint32_t seq = hdr.sequence;

    worker_pool_->submit([this, conn, uid, gid, target_uid, set_admin, seq]() {
        // 只有群主能操作
        if (get_group_role(gid, uid) != 2) {

            Buffer resp; 
            resp.write_int8(0); 
            resp.write_string("仅群主可设置管理员");

            std::string pkt = MessageHelper::pack_header(MsgType::GROUP_SET_ADMIN_ACK, resp.data().size(), seq);
            pkt.append(resp.data()); 
            
            conn->send_raw(pkt); 
            return;
        }
        // 目标必须是群成员
        if (!is_group_member(gid, target_uid)) {
            Buffer resp; 
            resp.write_int8(0); 
            resp.write_string("目标用户不在群中");

            std::string pkt = MessageHelper::pack_header(MsgType::GROUP_SET_ADMIN_ACK, resp.data().size(), seq);
            pkt.append(resp.data()); 
            
            conn->send_raw(pkt); 
            return;
        }

        auto* m = mysql_pool_->acquire();

        int new_role = set_admin ? 1 : 0;

        std::string q = "UPDATE group_members SET role=" + std::to_string(new_role)
                        + " WHERE group_id=" + std::to_string(gid)
                        + " AND user_id=" + std::to_string(target_uid);

        mysql_query(m, q.c_str());
        mysql_pool_->release(m);

        std::string tip = set_admin ? "你已被设为管理员" : "你已被取消管理员";

        Buffer n; 
        n.write_int64(gid); 
        n.write_string(tip);
        std::string np = MessageHelper::pack_header(MsgType::GROUP_NOTIFY, n.data().size(), 0);
        np.append(n.data()); 
        
        send_to_user(target_uid, np);

        Buffer resp; 
        resp.write_int8(1);
        resp.write_string(set_admin ? "已设为管理员" : "已取消管理员");

        std::string pkt = MessageHelper::pack_header(MsgType::GROUP_SET_ADMIN_ACK, resp.data().size(), seq);
        pkt.append(resp.data()); 
        
        conn->send_raw(pkt);

    });
}

// ======================== 群信息 ========================

void Server::handle_group_info(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {
    int64_t gid = body.read_int64();

    if (!conn->is_authenticated()) {
        return;
    }

    uint32_t seq = hdr.sequence;

    worker_pool_->submit([this, conn, gid, seq]() {

        auto* m = mysql_pool_->acquire();

        std::string q = "SELECT g.id, g.group_name, g.owner_id, u.username, g.description, g.created_at, "
                        "(SELECT COUNT(*) FROM group_members WHERE group_id=g.id) "
                        "FROM groups_info g JOIN users u ON g.owner_id=u.id WHERE g.id=" + std::to_string(gid);

        mysql_query(m, q.c_str());

        MYSQL_RES* res = mysql_store_result(m);

        if (!res) { 
            mysql_pool_->release(m); 
            return ; 
        }

        if (MYSQL_ROW row = mysql_fetch_row(res)) {

            Buffer resp; 
            resp.write_int8(1); 
            resp.write_int64(std::stoll(row[0]));
            resp.write_string(row[1] ? row[1] : "");
            resp.write_int64(row[2] ? std::stoll(row[2]) : 0);
            resp.write_string(row[3] ? row[3] : "");
            resp.write_string(row[4] ? row[4] : "");
            resp.write_int64(row[5] ? std::stoll(row[5]) : 0);
            resp.write_int32(row[6] ? std::stoi(row[6]) : 0);

            std::string pkt = MessageHelper::pack_header(MsgType::GROUP_INFO_RESPONSE, resp.data().size(), seq);
            pkt.append(resp.data()); 
            
            conn->send_raw(pkt);

        } 
        else {
            Buffer resp; 
            resp.write_int8(0); 
            resp.write_string("群不存在");

            std::string pkt = MessageHelper::pack_header(MsgType::GROUP_INFO_RESPONSE, resp.data().size(), seq);
            pkt.append(resp.data()); 
            
            conn->send_raw(pkt);
        }

        mysql_free_result(res); 
        mysql_pool_->release(m);

    });

}

// ======================== 我的群列表 ========================

void Server::handle_group_list(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {
    int64_t uid = conn->get_user_id();

    if (!conn->is_authenticated()) {
        return;
    }

    uint32_t seq = hdr.sequence;
    (void)body;

    worker_pool_->submit([this, conn, uid, seq]() {

        auto* m = mysql_pool_->acquire();

        std::string q = "SELECT g.id, g.group_name, g.owner_id, gm.role, "
                        "(SELECT COUNT(*) FROM group_members WHERE group_id=g.id) "
                        "FROM groups_info g JOIN group_members gm ON g.id=gm.group_id "
                        "WHERE gm.user_id=" + std::to_string(uid);
        
        if (mysql_query(m, q.c_str()) != 0) {
            LOG_ERROR("group_list query failed: {}", mysql_error(m));

            mysql_pool_->release(m);

            Buffer resp; resp.write_int32(0);

            std::string pkt = MessageHelper::pack_header(MsgType::GROUP_LIST_RESPONSE, resp.data().size(), seq);
            pkt.append(resp.data()); 
            
            conn->send_raw(pkt);
            return;
        }
        
        MYSQL_RES* res = mysql_store_result(m);
        if (!res) {

            LOG_ERROR("group_list store_result failed: {}", mysql_error(m));
            mysql_pool_->release(m);

            Buffer resp; 
            resp.write_int32(0);
            std::string pkt = MessageHelper::pack_header(MsgType::GROUP_LIST_RESPONSE, resp.data().size(), seq);

            pkt.append(resp.data()); 
            
            conn->send_raw(pkt);
            return;
        }
        
        int count = mysql_num_rows(res);
        Buffer resp; 
        resp.write_int32(count);

        MYSQL_ROW row;

        while ((row = mysql_fetch_row(res))) {

            resp.write_int64(std::stoll(row[0]));
            resp.write_string(row[1] ? row[1] : "");
            resp.write_int64(row[2] ? std::stoll(row[2]) : 0);
            resp.write_int8(row[3] ? std::stoi(row[3]) : 0);
            resp.write_int32(row[4] ? std::stoi(row[4]) : 0);
            
        }

        mysql_free_result(res); mysql_pool_->release(m);

        std::string pkt = MessageHelper::pack_header(MsgType::GROUP_LIST_RESPONSE, resp.data().size(), seq);
        pkt.append(resp.data()); 
        
        conn->send_raw(pkt);

    });

}

// ======================== 群成员列表 ========================

void Server::handle_group_members(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {
    int64_t gid = body.read_int64();

    if (!conn->is_authenticated()) {
        return;
    }

    uint32_t seq = hdr.sequence;

    worker_pool_->submit([this, conn, gid, seq]() {

        auto* m = mysql_pool_->acquire();

        std::string q = "SELECT u.id, u.username, u.nickname, u.status, gm.role "
                        "FROM group_members gm JOIN users u ON gm.user_id=u.id "
                        "WHERE gm.group_id=" + std::to_string(gid);

        mysql_query(m, q.c_str());
        MYSQL_RES* res = mysql_store_result(m);

        if (!res) { 
            mysql_pool_->release(m); 
            return ; 
        }

        int count = mysql_num_rows(res);

        Buffer resp; resp.write_int32(count);

        MYSQL_ROW row;

        while ((row = mysql_fetch_row(res))) {
            resp.write_int64(std::stoll(row[0]));
            resp.write_string(row[1] ? row[1] : "");
            resp.write_string(row[2] ? row[2] : "");
            resp.write_int8(row[3] ? std::stoi(row[3]) : 0);
            resp.write_int8(row[4] ? std::stoi(row[4]) : 0);
        }

        mysql_free_result(res); 
        mysql_pool_->release(m);

        std::string pkt = MessageHelper::pack_header(MsgType::GROUP_MEMBERS_RESPONSE, resp.data().size(), seq);
        pkt.append(resp.data()); 
        
        conn->send_raw(pkt);
    });
}

// ======================== 群消息 ========================

void Server::handle_group_message(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {
    int64_t uid = conn->get_user_id();

    if (!conn->is_authenticated()) {
        return;
    }

    int64_t gid = body.read_int64();
    std::string content = body.read_string();
    uint32_t seq = hdr.sequence;

    worker_pool_->submit([this, conn, uid, gid, content, seq]() {

        if (!is_group_member(gid, uid)) {
            Buffer resp; 
            resp.write_int8(0); 
            resp.write_string("不在群中");

            std::string pkt = MessageHelper::pack_header(MsgType::GROUP_MESSAGE_ACK, resp.data().size(), seq);
            pkt.append(resp.data()); 
            
            conn->send_raw(pkt); 
            return;

        }

        std::string msg_id = Crypto::generate_transfer_id();

        auto* m = mysql_pool_->acquire();

        std::string q = "INSERT INTO group_messages (message_id, group_id, sender_id, content, sent_at) VALUES ('"
                        + msg_id + "'," + std::to_string(gid) + "," + std::to_string(uid) + ",'"
                        + esc(m, content) + "'," + std::to_string(time(nullptr)) + ")";

        mysql_query(m, q.c_str());
        mysql_pool_->release(m);

        Buffer fwd; 
        fwd.write_int64(gid); 
        fwd.write_int64(uid);
        fwd.write_string(conn->get_user_info().username); 
        fwd.write_string(content);
        fwd.write_int64(time(nullptr));

        std::string pkt = MessageHelper::pack_header(MsgType::GROUP_MESSAGE, fwd.data().size(), 0);
        pkt.append(fwd.data());

        send_to_group(gid, uid, pkt);

        Buffer ack; 
        ack.write_int8(1); 
        ack.write_string(msg_id);

        std::string ap = MessageHelper::pack_header(MsgType::GROUP_MESSAGE_ACK, ack.data().size(), seq);
        ap.append(ack.data()); 
        
        conn->send_raw(ap);

        LOG_INFO("Group msg from {} to group {}", uid, gid);

    });

}

// ======================== 群历史消息 

void Server::handle_group_history(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {
    int64_t uid = conn->get_user_id();

    if (!conn->is_authenticated()) {
        return;
    }

    int64_t gid = body.read_int64();
    int64_t before_ts = body.end() ? 0 : body.read_int64();
    uint32_t seq = hdr.sequence;

    worker_pool_->submit([this, conn, uid, gid, before_ts, seq]() {

        if (!is_group_member(gid, uid)) {
            Buffer resp;
            resp.write_int32(0);
            std::string pkt = MessageHelper::pack_header(MsgType::GROUP_HISTORY_RESPONSE, resp.data().size(), seq);
            pkt.append(resp.data());
            conn->send_raw(pkt);
            return;
        }

        auto* m = mysql_pool_->acquire();

        std::string time_filter = before_ts > 0 ? " AND gm.sent_at < " + std::to_string(before_ts) : "";
        std::string ftime_filter = before_ts > 0 ? " AND ft.created_at < " + std::to_string(before_ts) : "";

        std::string q = "(SELECT gm.message_id, gm.sender_id, u.username, gm.content, gm.sent_at, 1 "
                        "FROM group_messages gm JOIN users u ON gm.sender_id=u.id "
                        "WHERE gm.group_id=" + std::to_string(gid) + time_filter + ") "
                        "UNION ALL "
                        "(SELECT ft.transfer_id, ft.sender_id, u2.username, ft.file_name, ft.created_at, 2 "
                        "FROM file_transfers ft JOIN users u2 ON ft.sender_id=u2.id "
                        "WHERE ft.group_id=" + std::to_string(gid) + " AND ft.status=1" + ftime_filter + ") "
                        "ORDER BY 5 DESC LIMIT 500";

        mysql_query(m, q.c_str());
        MYSQL_RES* res = mysql_store_result(m);

        if (!res) {
            mysql_pool_->release(m);
            return;
        }

        int count = mysql_num_rows(res);

        Buffer resp;
        resp.write_int32(count);

        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res))) {
            resp.write_string(row[0] ? row[0] : "");
            resp.write_int64(row[1] ? std::stoll(row[1]) : 0);
            resp.write_string(row[2] ? row[2] : "");
            resp.write_string(row[3] ? row[3] : "");
            resp.write_int64(row[4] ? std::stoll(row[4]) : 0);
            resp.write_int8(row[5] ? std::stoi(row[5]) : 1);
        }

        mysql_free_result(res);
        mysql_pool_->release(m);

        std::string pkt = MessageHelper::pack_header(MsgType::GROUP_HISTORY_RESPONSE, resp.data().size(), seq);
        pkt.append(resp.data());

        conn->send_raw(pkt);

    });

}

void Server::handle_heartbeat(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer&) {
    // 未认证连接也刷新心跳计时，避免握手后未登录的连接被超时误杀
    conn->update_heartbeat();
    heartbeat_timer_->update(conn->get_fd());

    if (!conn->is_authenticated()) {
        return;
    }

    std::string packet = MessageHelper::pack_header(MsgType::HEARTBEAT_ACK, 0, hdr.sequence);

    conn->send_raw(packet);

}

} // namespace chat