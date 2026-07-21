#ifndef CHAT_MESSAGE_H
#define CHAT_MESSAGE_H

#include <string>
#include <cstdint>
#include <cstring>
#include <ctime>

namespace chat {

enum class MsgType : uint16_t {
    LOGIN_REQUEST = 100, LOGIN_RESPONSE = 101,
    REGISTER_REQUEST = 102, REGISTER_RESPONSE = 103,
    LOGOUT_REQUEST = 104, LOGOUT_RESPONSE = 105,
    VERIFY_CODE_REQUEST = 106, VERIFY_CODE_RESPONSE = 107,
    PASSWORD_RESET_REQUEST = 108, PASSWORD_RESET_RESPONSE = 109,
    HEARTBEAT = 200, HEARTBEAT_ACK = 201,
    ERROR_RESPONSE = 999
};

#pragma pack(push, 1)
struct MsgHeader {
    uint32_t length;
    uint16_t version;
    MsgType  type;
    uint32_t sequence;
    uint64_t timestamp;
};
#pragma pack(pop)

class Buffer {
public:
    Buffer() {}
    explicit Buffer(const std::string& d) : data_(d) {}

    void write_int8(uint8_t v)    { data_.append((char*)&v, 1); }
    void write_int16(uint16_t v)  { data_.append((char*)&v, 2); }
    void write_int32(uint32_t v)  { data_.append((char*)&v, 4); }
    void write_int64(uint64_t v)  { data_.append((char*)&v, 8); }
    void write_string(const std::string& s) {
        write_int32((uint32_t)s.size());
        data_.append(s);
    }

    uint8_t  read_int8()   { uint8_t v;  memcpy(&v, &data_[pos_], 1); pos_+=1; return v; }
    uint16_t read_int16()  { uint16_t v; memcpy(&v, &data_[pos_], 2); pos_+=2; return v; }
    uint32_t read_int32()  { uint32_t v; memcpy(&v, &data_[pos_], 4); pos_+=4; return v; }
    uint64_t read_int64()  { uint64_t v; memcpy(&v, &data_[pos_], 8); pos_+=8; return v; }
    std::string read_string() {
        uint32_t len = read_int32();
        std::string s = data_.substr(pos_, len);
        pos_ += len;
        return s;
    }

    const std::string& data() const { return data_; }
    bool end() const { return pos_ >= data_.size(); }

private:
    std::string data_;
    size_t pos_ = 0;
};

struct MessageHelper {
    static std::string pack_header(MsgType type, uint32_t body_len, uint32_t seq = 0) {
        MsgHeader h;
        h.length    = sizeof(MsgHeader) + body_len;
        h.version   = 1;
        h.type      = type;
        h.sequence  = seq;
        h.timestamp = (uint64_t)time(nullptr);
        std::string s;
        s.append((char*)&h, sizeof(h));
        return s;
    }

    static bool unpack_header(const std::string& data, MsgHeader& h) {
        if (data.size() < sizeof(MsgHeader)) {
            return false;
        }

        memcpy(&h, data.data(), sizeof(MsgHeader));

        if (data.size() < h.length){
            return false;
        } 
        
        return true;
    }
};

} // namespace chat

#endif