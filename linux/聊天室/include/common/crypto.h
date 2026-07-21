
#define CHAT_CRYPTO_H

#include <string>

namespace chat {

class Crypto {
public:
    static std::string hash_password(const std::string& password);
    static bool verify_password(const std::string& password, const std::string& hash);
    static std::string generate_verify_code(int length = 6);
};

} // namespace chat
