#include "/home/lighning/codes/test/linux/聊天室/include/server/server.h"
#include "/home/lighning/codes/test/linux/聊天室/include/server/logger.h"
#include "/home/lighning/codes/test/linux/聊天室/include/common/message.h"
#include "/home/lighning/codes/test/linux/聊天室/include/common/crypto.h"
#include <chrono>
#include "/home/lighning/codes/test/linux/聊天室/include/common/email.h"
namespace chat {

Server::Server(int port, const std::string& address) : port_(port), address_(address) {}
Server::~Server() { stop(); }

bool Server::initialize() {
    try {
        mysql_pool_ = std::make_unique<MySQLPool>("127.0.0.1", 3306, "chat", "chat123", "chat_system", 20);
        redis_pool_ = std::make_unique<RedisPool>("127.0.0.1", 6379, "", 0, 10);
        heartbeat_timer_ = std::make_unique<HeartbeatTimer>(60);
        worker_pool_ = std::make_unique<WorkerPool>(4);
        for (int i = 0; i < 4; ++i) sub_reactors_.push_back(std::make_unique<SubReactor>(i));
        main_reactor_ = std::make_unique<MainReactor>(this, port_, address_);
        LOG_INFO("Server initialized");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Init failed: {}", e.what());
        return false;
    }
}

void Server::run() {
    running_ = true;
    worker_pool_->start();
    for (auto& r : sub_reactors_) r->run();
    main_reactor_->run();
    
    // 阻塞等待主线程
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void Server::stop() {
    if (!running_) return;
    running_ = false;
    worker_pool_->stop();
    for (auto& r : sub_reactors_) r->stop();
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
            LOG_INFO("User {} disconnected", uid);
            break;
        }
    }
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
    switch (hdr.type) {
        case MsgType::LOGIN_REQUEST:         handle_login(conn, hdr, body); break;
        case MsgType::REGISTER_REQUEST:      handle_register(conn, hdr, body); break;
        case MsgType::LOGOUT_REQUEST:        handle_logout(conn, hdr, body); break;
        case MsgType::VERIFY_CODE_REQUEST:   handle_verify_code(conn, hdr, body); break;
        case MsgType::PASSWORD_RESET_REQUEST:handle_password_reset(conn, hdr, body); break;
        case MsgType::HEARTBEAT:             handle_heartbeat(conn, hdr, body); break;
        default: break;
    }
}

// ==================== LOGIN ====================
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
            // ===== 验证码登录（username是邮箱）=====
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
                                    + username + "' AND code='" + password + "'";
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
        } else {
            // ===== 密码登录 =====
            std::string q = "SELECT id, username, password_hash, email, phone, nickname "
                            "FROM users WHERE username='" + username + "'";
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

        std::string q = "SELECT COUNT(*) FROM users WHERE username='" + username + "'";
        mysql_query(m, q.c_str());
        MYSQL_RES* res = mysql_store_result(m);
        MYSQL_ROW row = mysql_fetch_row(res);
        uint8_t success = 0;
        uint64_t uid = 0;

        if (row && std::stoi(row[0]) > 0) {
            // 用户名已存在
        } else {
            std::string hash = Crypto::hash_password(password);
            q = "INSERT INTO users (username, password_hash, email, phone, created_at) VALUES ('"
                + username + "','" + hash + "','" + email + "','" + phone + "',"
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

// ==================== LOGOUT ====================
void Server::handle_logout(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer&) {
    if (!conn->is_authenticated()) return;
    int64_t uid = conn->get_user_id();

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

// ==================== VERIFY CODE ====================
void Server::handle_verify_code(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {
    std::string target = body.read_string();
    uint8_t code_type = body.read_int8();
    std::string code = Crypto::generate_verify_code();
    uint32_t seq = hdr.sequence;

    worker_pool_->submit([this, conn, target, code_type, code, seq]() {
        auto* m = mysql_pool_->acquire();
        std::string q = "INSERT INTO verification_codes (target, code, type, expires_at) VALUES ('"
                        + target + "','" + code + "'," + std::to_string(code_type) + ","
                        + std::to_string(time(nullptr) + 300) + ")";
        mysql_query(m, q.c_str());
        mysql_pool_->release(m);

        // 发送邮件
        EmailSender::send_code(target, code);

        Buffer resp;
        resp.write_int8(1);
        std::string packet = MessageHelper::pack_header(MsgType::VERIFY_CODE_RESPONSE, resp.data().size(), seq);
        packet.append(resp.data());
        conn->send_raw(packet);
    });
}
// ==================== PASSWORD RESET ====================
void Server::handle_password_reset(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer& body) {
    std::string target = body.read_string();
    std::string code = body.read_string();
    std::string new_password = body.read_string();

    auto* m = mysql_pool_->acquire();
    std::string q = "SELECT COUNT(*) FROM verification_codes WHERE target='" + target
                    + "' AND code='" + code + "' AND used=0 AND expires_at>" + std::to_string(time(nullptr));
    mysql_query(m, q.c_str());
    MYSQL_RES* res = mysql_store_result(m);
    MYSQL_ROW row = mysql_fetch_row(res);
    uint8_t success = 0;

    if (row && std::stoi(row[0]) > 0) {
        std::string hash = Crypto::hash_password(new_password);
        q = "UPDATE users SET password_hash='" + hash + "' WHERE email='" + target + "' OR phone='" + target + "'";
        mysql_query(m, q.c_str());
        q = "UPDATE verification_codes SET used=1 WHERE target='" + target + "' AND code='" + code + "'";
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

// ==================== HEARTBEAT ====================
void Server::handle_heartbeat(std::shared_ptr<Connection> conn, const MsgHeader& hdr, Buffer&) {
    conn->update_heartbeat();
    heartbeat_timer_->update(conn->get_fd());
    std::string packet = MessageHelper::pack_header(MsgType::HEARTBEAT_ACK, 0, hdr.sequence);
    conn->send_raw(packet);
}

} // namespace chat