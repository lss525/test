#include "/home/lighning/codes/test/linux/聊天室/include/client/client.h"
#include <iostream>
using namespace chat;

void show_menu() {
    std::cout << "\n========== 聊天室 ==========\n";
    std::cout << "1. 登录\n";
    std::cout << "2. 注册\n";
    std::cout << "3. 退出\n";
    std::cout << "请选择: ";
}


void do_login(Client& client) {
    std::cout << "\n=== 登录 ===\n";
    std::cout << "1. 密码登录\n";
    std::cout << "2. 验证码登录\n";
    std::cout << "请选择: ";
    std::string choice;
    std::getline(std::cin, choice);

    if (choice == "1") {
        // 密码登录
        std::string username, password;
        std::cout << "用户名: ";
        std::getline(std::cin, username);
        std::cout << "密码: ";
        std::getline(std::cin, password);

        client.send_login(username, password);
    } 
   else if (choice == "2") {
    std::string email;
    std::cout << "QQ邮箱: ";
    std::getline(std::cin, email);

    client.send_verify_code(email, 2);

    MsgHeader hdr;
    Buffer body;
    if (client.wait_response(hdr, body, 10)) {
        if (body.read_int8() == 1) {
            std::string code;
            std::cout << "请输入验证码: ";
            std::getline(std::cin, code);

            client.send_login_with_code(email, code);

            // 等登录响应
            MsgHeader hdr2;
            Buffer body2;
            if (!client.wait_response(hdr2, body2, 10)) {
                std::cerr << "服务器无响应" << std::endl;
                return;
            }
            if (hdr2.type == MsgType::LOGIN_RESPONSE) {
                uint8_t success = body2.read_int8();
                if (success) {
                    std::cout << "登录成功\n";
                } else {
                    std::string err = body2.read_string();
                    std::cerr << "登录失败: " << err << std::endl;
                }
            }
            return;  // 直接返回，不走到外面的 wait_response
        } else {
            std::cout << "验证码发送失败\n";
            return;
        }
    }
}
    // 等待登录响应
    MsgHeader hdr;
    Buffer body;
    if (!client.wait_response(hdr, body, 10)) {
        std::cerr << "服务器无响应" << std::endl;
        return;
    }

    if (hdr.type == MsgType::LOGIN_RESPONSE) {
        uint8_t success = body.read_int8();
        if (success) {
            uint64_t uid = body.read_int64();
            std::string uname = body.read_string();
            std::string nick = body.read_string();
            std::string email = body.read_string();
            std::string phone = body.read_string();

            std::cout << "\n登录成功!\n";
            std::cout << "用户ID: " << uid << "\n";
            std::cout << "用户名: " << uname << "\n";
            std::cout << "昵称: " << nick << "\n";
        } else {
            std::string err = body.read_string();
            std::cerr << "登录失败: " << err << std::endl;
        }
    }
}
void do_register(Client& client) {
    std::string username, password, email, phone;
    std::cout << "用户名: ";
    std::getline(std::cin, username);
    std::cout << "密码: ";
    std::getline(std::cin, password);
    std::cout << "邮箱: ";
    std::getline(std::cin, email);
    std::cout << "手机号: ";
    std::getline(std::cin, phone);

    client.send_register(username, password, email, phone);

    MsgHeader hdr;
    Buffer body;
    if (!client.wait_response(hdr, body, 5)) {
        std::cerr << "服务器无响应" << std::endl;
        return;
    }

    if (hdr.type == MsgType::REGISTER_RESPONSE) {
        uint8_t success = body.read_int8();
        uint64_t uid = body.read_int64();
        if (success) {
            std::cout << "\n注册成功! 用户ID: " << uid << "\n";
        } else {
            std::cerr << "注册失败: 用户名已存在" << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "用法: " << argv[0] << " <服务器IP> <端口>" << std::endl;
        std::cerr << "示例: " << argv[0] << " 127.0.0.1 8888" << std::endl;
        return 1;
    }

    std::string host = argv[1];
    int port = std::stoi(argv[2]);

    Client client(host, port);
    if (!client.connect()) {
        return 1;
    }

    std::string choice;
    while (client.is_connected()) {
        show_menu();
        std::getline(std::cin, choice);

        if (choice == "1") {
            do_login(client);
        } else if (choice == "2") {
            do_register(client);
        } else if (choice == "3") {
            std::cout << "再见!\n";
            break;
        }
    }

    client.disconnect();
    return 0;
}