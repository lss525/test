#include "../include/common/crypto.h"
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <sstream>
#include <iomanip>

namespace chat {

std::string Crypto::hash_password(const std::string& password) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(password.c_str()), password.size(), hash);
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return ss.str();
}

bool Crypto::verify_password(const std::string& password, const std::string& hash) {
    return hash_password(password) == hash;
}

std::string Crypto::generate_verify_code(int length) {
    std::string code;
    unsigned char buf[1];
    for (int i = 0; i < length; ++i) {
        RAND_bytes(buf, 1);
        code += '0' + (buf[0] % 10);
    }
    return code;
}

std::string Crypto::generate_transfer_id() {
    unsigned char buf[16];
    RAND_bytes(buf, sizeof(buf));
    std::stringstream ss;
    for (int i = 0; i < 16; i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)buf[i];
    return ss.str();
}

} // namespace chat