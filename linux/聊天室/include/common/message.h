#ifndef CHAT_MESSAGE_H
#define CHAT_MESSAGE_H

#include <string>
#include <cstdint>
#include <cstring>
#include <ctime>

namespace chat {

enum class MsgType : uint16_t {
    // 认证
    LOGIN_REQUEST = 100, LOGIN_RESPONSE = 101,
    REGISTER_REQUEST = 102, REGISTER_RESPONSE = 103,
    LOGOUT_REQUEST = 104, LOGOUT_RESPONSE = 105,
    VERIFY_CODE_REQUEST = 106, VERIFY_CODE_RESPONSE = 107,
    PASSWORD_RESET_REQUEST = 108, PASSWORD_RESET_RESPONSE = 109,

    // 好友
    FRIEND_ADD_REQUEST = 200, FRIEND_ADD_RESPONSE = 201,
    FRIEND_DELETE_REQUEST = 202, FRIEND_DELETE_RESPONSE = 203,
    FRIEND_LIST_REQUEST = 204, FRIEND_LIST_RESPONSE = 205,
    FRIEND_BLOCK_REQUEST = 206, FRIEND_BLOCK_RESPONSE = 207,
    FRIEND_STATUS_NOTIFY = 208,
    FRIEND_UNBLOCK_REQUEST = 209, FRIEND_UNBLOCK_RESPONSE = 210,

    // 私聊
    PRIVATE_MESSAGE = 300, PRIVATE_MESSAGE_ACK = 301,
    PRIVATE_HISTORY_REQUEST = 302, PRIVATE_HISTORY_RESPONSE = 303,
    OFFLINE_MESSAGE_NOTIFY = 304,

    // 文件
    FILE_TRANSFER_INIT = 600, FILE_TRANSFER_INIT_ACK = 601,
    FILE_TRANSFER_CHUNK = 602, FILE_TRANSFER_COMPLETE = 603,
    FILE_DOWNLOAD_REQUEST = 604, FILE_DOWNLOAD_RESPONSE = 605,

    // 系统
    HEARTBEAT = 700, HEARTBEAT_ACK = 701,

    // 群组
    GROUP_CREATE_REQUEST = 800, GROUP_CREATE_RESPONSE = 801,
    GROUP_DISSOLVE_REQUEST = 802, GROUP_DISSOLVE_RESPONSE = 803,
    GROUP_JOIN_REQUEST = 804, GROUP_JOIN_RESPONSE = 805,
    GROUP_QUIT_REQUEST = 806, GROUP_QUIT_RESPONSE = 807,
    GROUP_KICK_REQUEST = 808, GROUP_KICK_RESPONSE = 809,
    GROUP_INFO_REQUEST = 810, GROUP_INFO_RESPONSE = 811,
    GROUP_LIST_REQUEST = 812, GROUP_LIST_RESPONSE = 813,
    GROUP_MEMBERS_REQUEST = 814, GROUP_MEMBERS_RESPONSE = 815,
    GROUP_SET_ADMIN = 816, GROUP_SET_ADMIN_ACK = 817,
    GROUP_MESSAGE = 820, GROUP_MESSAGE_ACK = 821,
    GROUP_HISTORY_REQUEST = 822, GROUP_HISTORY_RESPONSE = 823,
    GROUP_NOTIFY = 840,
    GROUP_JOIN_REQ_NOTIFY = 841,
    GROUP_JOIN_REQ_HANDLE = 842, GROUP_JOIN_REQ_HANDLE_ACK = 843,

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
        if (data.size() < sizeof(MsgHeader)) return false;
        memcpy(&h, data.data(), sizeof(MsgHeader));
        if (data.size() < h.length) return false;
        return true;
    }
};

} // namespace chat

#endif
