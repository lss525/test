#include "../include/client/client.h"
#include <iostream>
using namespace chat;

void show_menu() {
    std::cout << "\n========== 聊天室 ==========\n1. 登录\n2. 注册\n3. 退出\n请选择: ";
}

void main_menu(Client& client);
void group_menu(Client& client);

// 安全的数字输入函数
int64_t input_int64(const std::string& prompt) {
    int64_t val;
    std::cout << prompt;

    while (!(std::cin >> val)) {
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        std::cout << "请输入数字: ";
    }

    std::cin.ignore();
    return val;
}

void handle_other_msg(MsgType t, Buffer& b) {
    if (t == MsgType::FILE_TRANSFER_INIT) {
        std::string tid = b.read_string();
        int64_t sid = b.read_int64();

        std::string fname = b.read_string();
        int64_t fsize = b.read_int64();

        std::cout << "   收到离线文件: " << fname << " (" << fsize << "字节)\n";
        std::cout << "   文件ID: " << tid << "\n";
        std::cout << "   请用选项9下载\n";

    }
    else if (t == MsgType::PRIVATE_MESSAGE) {
        b.read_string();
        int64_t sid = b.read_int64();

        std::string sname = b.read_string();
        std::string content = b.read_string();

        b.read_int64();

        std::cout << "\n   " << sname << ": " << content << "\n";

    }
    else if (t == MsgType::GROUP_MESSAGE) {
        int64_t gid = b.read_int64();
        int64_t sid = b.read_int64();

        std::string sname = b.read_string();
        std::string content = b.read_string();

        b.read_int64();
        std::cout << "\n   [群" << gid << "] " << sname << ": " << content << "\n";
    }
    else if (t == MsgType::GROUP_NOTIFY) {
        int64_t gid = b.read_int64();
        std::string msg = b.read_string();

        std::cout << "\n   群通知 [群" << gid << "]: " << msg << "\n";
    }
    else if (t == MsgType::GROUP_JOIN_REQ_NOTIFY) {
        int64_t gid = b.read_int64();
        int64_t req_id = b.read_int64();
        int64_t uid = b.read_int64();

        std::string uname = b.read_string();
        std::string msg = b.read_string();

        std::cout << "\n 加群申请 [群" << gid << "] " << uname
                  << "(ID:" << uid << ") 申请ID:" << req_id << " 留言:" << msg << "\n";
    }
    else if (t == MsgType::FRIEND_ADD_REQUEST) {
        int64_t uid = b.read_int64();

        std::string uname = b.read_string();

        std::cout << "\n 好友请求: " << uname << "(ID:" << uid << ") 请求添加你为好友\n";
    }
    else if (t == MsgType::OFFLINE_MESSAGE_NOTIFY) {
        b.read_string();
        int64_t sid = b.read_int64();

        std::string sname = b.read_string();
        std::string content = b.read_string();

        b.read_int64();
        std::cout << "\n 离线消息 from " << sname << ": " << content << "\n";
    }
}

void do_login(Client& client) {
    std::cout << "\n=== 登录 ===\n1. 密码登录\n2. 验证码登录\n请选择: ";

    std::string c; 
    std::getline(std::cin, c);

    if (c == "1") {
        std::string u, p;

        std::cout << "用户名: "; std::getline(std::cin, u);
        std::cout << "密码: "; std::getline(std::cin, p);

        client.send_login(u, p);

        while (true) {
            MsgType type; 
            Buffer body;

            if (!client.wait_msg(type, body, 10)) { 
                std::cerr << "超时\n"; 
                return; 
            }

            if (type == MsgType::LOGIN_RESPONSE) {

                if (body.read_int8()) {
                    uint64_t uid = body.read_int64();
                    std::string un = body.read_string();

                    body.read_string(); 
                    body.read_string(); 
                    body.read_string();

                    std::cout << "\n登录成功! ID:" << uid << " 用户:" << un << "\n";
                    client.set_user_info(uid, un);

                    main_menu(client);
                } 
                else { 
                    std::cerr << "失败:" << body.read_string() << "\n"; 
                }
                
                return;
            }
            handle_other_msg(type, body);
        }
    } 
    else if (c == "2") {
        std::string email;
        std::cout << "QQ邮箱: "; std::getline(std::cin, email);

        client.send_verify_code(email, 2);

        while (true) {
            MsgType type; Buffer body;

            if (!client.wait_msg(type, body, 10)) { 
                std::cerr << "超时\n"; return; 
            }

            if (type == MsgType::VERIFY_CODE_RESPONSE) {
                if (body.read_int8()) {
                    break;
                }
                else { 
                    std::cerr << "验证码发送失败\n"; 
                    return; 
                }
            }
            
        }
        std::string code;
        std::cout << "验证码: "; std::getline(std::cin, code);
        client.send_login_with_code(email, code);

        while (true) {
            MsgType type; 
            Buffer body;

            if (!client.wait_msg(type, body, 10)) { 
                std::cerr << "超时\n"; 
                return; 
            }

            if (type == MsgType::LOGIN_RESPONSE) {
                if (body.read_int8()) {
                    uint64_t uid = body.read_int64();
                    std::string un = body.read_string();

                    body.read_string(); 
                    body.read_string(); 
                    body.read_string();

                    std::cout << "\n登录成功! ID:" << uid << " 用户:" << un << "\n";
                    client.set_user_info(uid, un);
                    main_menu(client);
                } 
                else { 
                    std::cerr << "失败:" << body.read_string() << "\n";
                }
                
                return;
            }
            handle_other_msg(type, body);
        }
    }
}

void do_register(Client& client) {
    std::string u, p, e, ph;

    std::cout << "用户名: "; std::getline(std::cin, u);
    std::cout << "密码: "; std::getline(std::cin, p);
    std::cout << "邮箱: "; std::getline(std::cin, e);
    std::cout << "手机号: "; std::getline(std::cin, ph);

    client.send_register(u, p, e, ph);

    MsgType type; 
    Buffer body;

    if (client.wait_msg(type, body, 5) && type == MsgType::REGISTER_RESPONSE) {
        std::cout << (body.read_int8() ? "注册成功 ID:" + std::to_string(body.read_int64()) : "注册失败") << "\n";
    }
}

// 群组子菜单

void group_menu(Client& client) {
    std::string gc;

    while (client.is_connected()) {

        MsgType t; 
        Buffer b;

        if (client.wait_msg(t, b, 0)) handle_other_msg(t, b);

        std::cout << "\n===== 群组菜单 =====\n"
                  << "1.我的群列表 2.创建群 3.群信息 4.群成员\n"
                  << "5.申请加群 6.处理申请 7.群消息 8.群历史\n"
                  << "9.群文件 10.下载文件 11.退出群 12.解散群 13.踢人 14.设置管理员 15.返回\n请选择: ";

        std::cout.flush();
        std::getline(std::cin, gc);

        if (gc == "1") {
            client.send_group_list();

            while (true) { 
                MsgType t; 
                Buffer b; 

                if (!client.wait_msg(t, b, 5)) {
                    break;
                }

                if (t == MsgType::GROUP_LIST_RESPONSE) {
                    int n = b.read_int32(); std::cout << "\n我的群(" << n << "):\n";

                    for (int i = 0; i < n; i++) {
                        int64_t id = b.read_int64(); std::string nm = b.read_string();
                        int64_t oid = b.read_int64(); int role = b.read_int8();
                        
                        int cnt = b.read_int32();

                        std::string rs = (role == 2 ? "群主" : (role == 1 ? "管理" : "成员"));
                        std::cout << id << "." << nm << " " << rs << "(" << cnt << "人)\n";
                    } 

                    break;
                } 
                else {
                    handle_other_msg(t, b);
                }
            }
        } 
        else if (gc == "2") {
            std::string nm, desc;

            std::cout << "群名: "; std::getline(std::cin, nm);
            std::cout << "描述: "; std::getline(std::cin, desc);

            client.send_group_create(nm, desc);

            while (true) { 
                MsgType t; 
                Buffer b; 

                if (!client.wait_msg(t, b, 5)) {
                    break;
                }

                if (t == MsgType::GROUP_CREATE_RESPONSE) {

                    if (b.read_int8()) {
                        std::cout << "正确 群ID:" << b.read_int64() << " 群名:" << b.read_string() << "\n";
                    }
                    else {
                         std::cout << "错误 " << b.read_string() << "\n"; break; 
                        }
                } 
                else {
                    handle_other_msg(t, b);
                }
            }
        } 
        else if (gc == "3") {
            int64_t gid = input_int64("群ID: ");
            client.send_group_info(gid);

            while (true) { 
                MsgType t;
                Buffer b; 
                if (!client.wait_msg(t, b, 5)) {
                    break;
                }

                if (t == MsgType::GROUP_INFO_RESPONSE) {
                    if (b.read_int8()) {
                        std::cout << "ID:" << b.read_int64() << " 群名:" << b.read_string()
                                  << " 群主ID:" << b.read_int64() << " 群主:" << b.read_string()
                                  << " 描述:" << b.read_string();

                                  b.read_int64(); 
                                  std::cout << " 人数:" << b.read_int32() << "\n";

                    } 
                    else {
                        std::cout << "错误 " << b.read_string() << "\n"; 
                        break;
                    }

                } 
                else {
                    handle_other_msg(t, b);
                }
            }
        } 
        else if (gc == "4") {
            int64_t gid = input_int64("群ID: ");
            client.send_group_members(gid);

            while (true) { 
                MsgType t; 
                Buffer b; 
                if (!client.wait_msg(t, b, 5)) {
                    break;
                }
                if (t == MsgType::GROUP_MEMBERS_RESPONSE) {
                    int n = b.read_int32(); 
                    std::cout << "\n成员(" << n << "):\n";

                    for (int i = 0; i < n; i++) {
                        int64_t id = b.read_int64(); std::string nm = b.read_string();
                        std::string nn = b.read_string(); int st = b.read_int8();
                        int role = b.read_int8();

                        std::string rs = (role == 2 ? "群主" : (role == 1 ? "管理" : "成员"));
                        std::cout << id << "." << nm << "(" << nn << ") " << (st ? "在线" : "离线") << " " << rs << "\n";

                    } 
                    break;

                } 
                else {
                    handle_other_msg(t, b);
                }
            }
        } 
        else if (gc == "5") {
            int64_t gid = input_int64("群ID: ");
            std::string msg;
            std::cout << "留言: "; std::getline(std::cin, msg);

            client.send_group_join(gid, msg);
            while (true) { 
                MsgType t; 
                Buffer b;

                if (!client.wait_msg(t, b, 5)) {
                    break;
                }
                if (t == MsgType::GROUP_JOIN_RESPONSE) {
                    std::cout << (b.read_int8() ? "正确" : "错误") << b.read_string() << "\n"; 
                    break;
                } 
                else handle_other_msg(t, b);
            }
        } 
        else if (gc == "6") {
            int64_t rid = input_int64("申请ID: ");
            uint8_t ap = input_int64("1同意 0拒绝: ");

            client.send_group_join_req_handle(rid, ap);
            while (true) { 
                MsgType t; 
                Buffer b; 
                if (!client.wait_msg(t, b, 5)) {
                    break;
                }
                if (t == MsgType::GROUP_JOIN_REQ_HANDLE_ACK) {
                    std::cout << (b.read_int8() ? "正确" : "错误") << b.read_string() << "\n"; 
                    break;
                } 
                else {
                    handle_other_msg(t, b);
                }
            }
        } 
        else if (gc == "7") {
            std::string msg;
            int64_t gid = input_int64("群ID: ");
            std::cout << "消息: "; std::getline(std::cin, msg);
            client.send_group_message(gid, msg);

            while (true) { 
                MsgType t; 
                Buffer b; 
                if (!client.wait_msg(t, b, 5)) {
                    break;
                }

                if (t == MsgType::GROUP_MESSAGE_ACK) {
                    std::cout << (b.read_int8() ? "正确" : "错误") << b.read_string() << "\n"; 
                    break;
                } 
                else {
                    handle_other_msg(t, b);
                }
            }
        } 
        else if (gc == "8") {
            int64_t gid = input_int64("群ID: ");
            client.send_group_history(gid);

            while (true) { 
                MsgType t; 
                Buffer b; 
                if (!client.wait_msg(t, b, 5)) {
                    break;
                }
                if (t == MsgType::GROUP_HISTORY_RESPONSE) {
                    int n = b.read_int32(); std::cout << "\n历史(" << n << "):\n";
                    for (int i = 0; i < n; i++) {
                        b.read_string(); int64_t sid = b.read_int64();
                        std::string sn = b.read_string(), ct = b.read_string(); b.read_int64();
                        std::cout << sn << ": " << ct << "\n";
                    } 
                    break;

                } 
                else {
                    handle_other_msg(t, b);
                }
            }
       } else if (gc == "9") {
            int64_t gid = input_int64("群ID: ");
            std::string path; std::cout << "文件路径: "; std::getline(std::cin, path);

            bool ok = client.send_group_file(path, gid);
            if (ok) std::cout << "群文件发送完成\n";

        } 
        else if (gc == "10") {
            std::string tid, path;

            std::cout << "文件ID: "; std::getline(std::cin, tid);
            std::cout << "保存路径: "; std::getline(std::cin, path);

            client.download_file(tid, path);
        } 
        else if (gc == "11") {
            int64_t gid = input_int64("群ID: ");
            client.send_group_quit(gid);

            while (true) { 
                MsgType t; 
                Buffer b; 
                if (!client.wait_msg(t, b, 5)) {
                    break;
                }
                if (t == MsgType::GROUP_QUIT_RESPONSE) {
                    std::cout << (b.read_int8() ? "正确" : "错误") << b.read_string() << "\n"; 
                    break;
                } 
                else {
                    handle_other_msg(t, b);
                }
            }
        } 
        else if (gc == "12") {
            int64_t gid = input_int64("群ID: ");
            client.send_group_dissolve(gid);

            while (true) { 
                MsgType t; 
                Buffer b; 
                if (!client.wait_msg(t, b, 5)) {
                    break;
                }
                if (t == MsgType::GROUP_DISSOLVE_RESPONSE) {
                    std::cout << (b.read_int8() ? "正确" : "错误") << b.read_string() << "\n"; 
                    break;
                } 
                else {
                    handle_other_msg(t, b);
                }
            }
        } 
        else if (gc == "13") {
            int64_t gid = input_int64("群ID: "); 
            int64_t tid = input_int64("目标用户ID: ");
            client.send_group_kick(gid, tid);

            while (true) { 
                MsgType t; 
                Buffer b; 
                if (!client.wait_msg(t, b, 5)) {
                    break;
                }

                if (t == MsgType::GROUP_KICK_RESPONSE) {
                    std::cout << (b.read_int8() ? "正确" : "错误") << b.read_string() << "\n"; break;
                } 
                else {
                    handle_other_msg(t, b);
                }
            }
        } 
        else if (gc == "14") {
	        int64_t gid = input_int64("群ID: "); 
            int64_t tid = input_int64("目标用户ID: "); 
            uint8_t set = input_int64("1设为管理 0取消: ");

            client.send_group_set_admin(gid, tid, set);

            while (true) { 
                MsgType t; 
                Buffer b; 
                if (!client.wait_msg(t, b, 5)) {
                    break;
                }

                if (t == MsgType::GROUP_SET_ADMIN_ACK) {
                    std::cout << (b.read_int8() ? "正确" : "错误") << b.read_string() << "\n"; break;
                } 
                else {
                    handle_other_msg(t, b);
                }

            }
        } 
        else if (gc == "15") {
            break;
        }
    }
}

// ======================== 主菜单 ========================

void main_menu(Client& client) {
    std::string c;
    while (client.is_connected()) {
        MsgType t; 
        Buffer b;

        while (client.wait_msg(t, b, 0)) {
            handle_other_msg(t, b);
        }

        std::cout << "\n===== 主菜单 =====\n"
                  << "1.好友列表 2.添加好友 3.删除好友 4.屏蔽好友 5.解除屏蔽\n"
                  << "6.私聊 7.聊天记录 8.发送文件 9.下载文件 10.群组 11.注销\n请选择: ";

        std::getline(std::cin, c);

        if (c == "1") {
            client.send_friend_list();
            while (true) { 
                MsgType t; 
                Buffer b; 

                if (!client.wait_msg(t, b, 5)) {
                    break;
                }

                if (t == MsgType::FRIEND_LIST_RESPONSE) {
                    int n = b.read_int32(); std::cout << "\n好友列表(" << n << "):\n";

                    for (int i = 0; i < n; i++) {
                        int64_t id = b.read_int64(); 
                        std::string nm = b.read_string(), nn = b.read_string();
                        int st = b.read_int8(), bl = b.read_int8();

                        std::cout << id << "." << nm << "(" << nn << ") " << (st ? "在线" : "离线") << (bl ? "[屏蔽]" : "") << "\n";
                    } 
                    break;
                } 
                else {
                    handle_other_msg(t, b);
                }
            }
        } 
        else if (c == "2") {
            std::string nm; std::cout << "好友用户名: "; 
            std::getline(std::cin, nm); 
            client.send_friend_add(nm);

            while (true) { 

                MsgType t; 
                Buffer b; 

                if (!client.wait_msg(t, b, 5)) {
                    break;
                }

                if (t == MsgType::FRIEND_ADD_RESPONSE) {
                    std::cout << (b.read_int8() ? "正确" : "错误") << b.read_string() << "\n"; break;
                } 
                else {
                    handle_other_msg(t, b);
                }
            }
        } 
        else if (c == "3") {
            int64_t id = input_int64("好友ID: ");
            client.send_friend_delete(id);

            while (true) { 
                MsgType t; 
                Buffer b; 
                if (!client.wait_msg(t, b, 5)) {
                    break;
                }
                if (t == MsgType::FRIEND_DELETE_RESPONSE) {
                    std::cout << (b.read_int8() ? "正确" : "错误") << b.read_string() << "\n"; break;
                } 
                else {
                    handle_other_msg(t, b);
                }
            }
        } 
        else if (c == "4") {

            int64_t id = input_int64("好友ID: ");
            client.send_friend_block(id);

            while (true) { MsgType t; Buffer b; if (!client.wait_msg(t, b, 5)) {
                break;
            }
                if (t == MsgType::FRIEND_BLOCK_RESPONSE) {
                    std::cout << (b.read_int8() ? "正确" : "错误") << b.read_string() << "\n"; 
                    break;
                } 
                else {
                    handle_other_msg(t, b);
                }
            }

        } 
        else if (c == "5") {
            int64_t id = input_int64("好友ID: ");
            client.send_friend_unblock(id);

            while (true) { 
                MsgType t;
                Buffer b; 
                if (!client.wait_msg(t, b, 5)) {
                    break;
                }

                if (t == MsgType::FRIEND_UNBLOCK_RESPONSE) {
                    std::cout << (b.read_int8() ? "正确" : "错误") << b.read_string() << "\n"; 
                    break;
                } 
                else {
                    handle_other_msg(t, b);
                }
            } 
        }
        else if (c == "6") {
            int64_t id = input_int64("发送给(ID): ");
            std::string msg;
            std::cout << "消息: "; std::getline(std::cin, msg); client.send_private_message(id, msg);
            while (true) { 
                MsgType t; 
                Buffer b;
                if (!client.wait_msg(t, b, 5)) {
                    break;
                }
                if (t == MsgType::PRIVATE_MESSAGE_ACK) {
                    std::cout << (b.read_int8() ? "正确" : "错误") << b.read_string() << "\n"; 
                    break;
                } 
                else{
                    handle_other_msg(t, b);
                }
            }
        } 
        else if (c == "7") {
            int64_t id = input_int64("好友ID: "); 
            client.send_private_history(id);
            while (true) { 
                MsgType t; 
                Buffer b; 
                if (!client.wait_msg(t, b, 5)) {
                    break;
                }
                if (t == MsgType::PRIVATE_HISTORY_RESPONSE) {
                    int n = b.read_int32(); std::cout << "\n聊天记录(" << n << "):\n";

                    for (int i = 0; i < n; i++) {
                        b.read_string(); int64_t sid = b.read_int64();
                        std::string ct = b.read_string();
                        b.read_int8(); b.read_int64();
                        std::cout << (sid == client.get_user_id() ? "[我]" : "[对方]") << ct << "\n";
                    } 
                    break;
                } else {
                    handle_other_msg(t, b);
                }
            }
        } 
        else if (c == "8") {
            int64_t to = input_int64("好友ID: ");
            std::string path; std::cout << "文件路径: "; std::getline(std::cin, path);

            bool ok = client.send_file(path, to, 0);
            if (ok) std::cout << "文件发送完成\n";
        }
        else if (c == "9") {
            std::string tid, path;

            std::cout << "文件ID: "; std::getline(std::cin, tid);
            std::cout << "保存路径: "; std::getline(std::cin, path);

            client.download_file(tid, path);
        } 
        else if (c == "10") {
            group_menu(client);
        } 
        else if (c == "11") {
            client.send_logout(); 
            std::cout << "已注销\n"; 
            break;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "用法: " << argv[0] << " <IP> <端口>\n"; return 1;
    }

    Client client(argv[1], std::stoi(argv[2]));

    if (!client.connect()) {
        std::cerr << "初始连接失败\n"; return 1;
    }
    client.start_heartbeat();
    client.start_recv_thread();

    std::string c;
    bool logged_in = false;

    while (true) {
        if (!client.is_connected()) {
            std::cout << "连接断开，5秒后重连...\n";
            sleep(5);
            if (client.connect()) {
                client.start_heartbeat();
                client.start_recv_thread();
                std::cout << "已重连，请重新登录\n";
                logged_in = false;
            } else {
                std::cout << "重连失败，继续尝试...\n";
                continue;
            }
        }

        show_menu();
        std::getline(std::cin, c);

        if (c == "1") {
            do_login(client);
            if (client.get_user_id() > 0) logged_in = true;
        }
        else if (c == "2") {
            do_register(client);
        }
        else if (c == "3") {
            break;
        }
    }
    client.disconnect();
    return 0;
}