#ifndef CHAT_USER_H
#define CHAT_USER_H

#include <string>
#include <cstdint>

namespace chat {

struct UserInfo {
    int64_t id = 0;
    std::string username;
    std::string password_hash;
    std::string email;
    std::string phone;
    std::string nickname;
    int status = 0;
};

} // namespace chat

#endif