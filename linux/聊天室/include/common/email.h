
#ifndef CHAT_EMAIL_H
#define CHAT_EMAIL_H

#include <string>

namespace chat {

class EmailSender {
public:
    static bool send_code(const std::string& to, const std::string& code);
};

} // namespace chat

#endif