#include "../include/server/connection.h"
#include "../include/server/reactor.h"
#include "../include/server/server.h"
#include "../include/server/logger.h"
#include <unistd.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

// 单个消息包的最大长度（防止恶意 length 导致内存膨胀/协议错乱）
static const size_t MAX_PACKET_SIZE = 1 * 1024 * 1024;   // 1MB


//------------------------------------------------------

#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>


static std::string base64_encode(const unsigned char* data, size_t len) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, mem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, data, len);
    BIO_flush(b64);
    
    char* buf;
    long size = BIO_get_mem_data(mem, &buf);
    std::string result(buf, size);
    BIO_free_all(b64);
    return result;
}

//--------------------------------------------------------


namespace chat {

Connection::Connection(int fd, SubReactor* reactor, Server* server)
    : fd_(fd), reactor_(reactor), server_(server) {

    last_heartbeat_ = time(nullptr);

}

Connection::~Connection() { 
    close(fd_); 
}

void Connection::handle_read() {
    char buf[65536];

    while (true) {

        int n = recv(fd_, buf, sizeof(buf), 0);

        if (n > 0) {
            // // recv
            read_buffer_.append(buf, n);
        } 
        else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
            handle_close();
            return;
        } 
        else {
            break;
        }

    }

    
//--- WebSocket 握手检测---------------------------
        // WS handshake debug disabled
        
          if (!ws_handshake_done_ && read_buffer_.find("HTTP/1") != std::string::npos && read_buffer_.find("Sec-WebSocket-Key:") == std::string::npos) {
        
            FILE* f = fopen("../web/index.html", "r");
        
            std::string body;
        
            if (f) { char b2[4096]; while (fgets(b2, sizeof(b2), f)) body += b2; fclose(f); }
        
                std::string html = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: " 
                                    + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
        
                int total = html.size();
                int sent = 0;
        
                while (sent < total) {
            
                    int n = send(fd_, html.c_str() + sent, total - sent, 0);
                    if (n <= 0) {
                        break;
                    }

                    sent += n;
        
                }
                // HTTP response sent
                read_buffer_.clear();
        
                return;
    
            }

    if (!ws_handshake_done_ && read_buffer_.find("Upgrade") != std::string::npos && read_buffer_.find("ebSocket") != std::string::npos) {
        if (try_ws_handshake()) {

            is_websocket_ = true;
            // WS handshake OK

            ws_handshake_done_ = true;
            read_buffer_.clear();

            return;

        }
    }

    if (is_websocket_) {

        process_ws_frame();
        // WS recv

        return;
    }

//----------------------------------------------------------

    while (read_buffer_.size() >= sizeof(MsgHeader)) {
        MsgHeader hdr;

        memcpy(&hdr, read_buffer_.data(), sizeof(MsgHeader));

        // 校验包长合法性，防止恶意 length 导致内存膨胀或协议错乱
        if (hdr.length < sizeof(MsgHeader) || hdr.length > MAX_PACKET_SIZE) {
            handle_close();
            return;
        }

        if (read_buffer_.size() < hdr.length) {
            break;
        }
        
        std::string body_data = read_buffer_.substr(sizeof(MsgHeader), hdr.length - sizeof(MsgHeader));

        read_buffer_ = read_buffer_.substr(hdr.length);

        Buffer body(body_data);

        server_->dispatch(shared_from_this(), hdr, body);
    }
}

void Connection::handle_write() { 
    do_write(); 
}

void Connection::handle_close() {

    reactor_->remove_connection(fd_);
    server_->on_connection_close(fd_);
}

void Connection::send_raw(const std::string& data) {

    if (is_websocket_) {
        send_ws(data);
        return;
    }

    std::lock_guard<std::mutex> lock(write_mutex_);
    write_buffer_ += data;

    if (!writing_) {
        writing_ = true;
        reactor_->modify_event(fd_, EPOLLIN | EPOLLOUT);
    }

}

void Connection::do_write() {

    std::lock_guard<std::mutex> lock(write_mutex_);

    while (!write_buffer_.empty()) {

        int n = send(fd_, write_buffer_.data(), write_buffer_.size(), 0);
        if (n > 0) {
            write_buffer_ = write_buffer_.substr(n);
        }

        else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) { 
            writing_ = false; return; 
        }

        else {
            break;
        }

    }

    if (write_buffer_.empty()) {

        writing_ = false;
        reactor_->modify_event(fd_, EPOLLIN);
    }

}

bool Connection::try_ws_handshake() {
    // 提取 Sec-WebSocket-Key
    std::string key;
    size_t pos = read_buffer_.find("Sec-WebSocket-Key: ");

    if (pos == std::string::npos) {
        return false;
    }

    pos += 19;

    size_t end = read_buffer_.find("\r\n", pos);
    key = read_buffer_.substr(pos, end - pos);

    while (!key.empty() && key.back() == ' ') {
        key.pop_back();
    }
    
    // 计算 Accept
    std::string magic = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    unsigned char hash[20];

    SHA1((unsigned char*)magic.c_str(), magic.size(), hash);
    std::string accept = base64_encode(hash, 20);
    
    

    // 构造握手响应
    std::string response = 
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";
    
    send(fd_, response.c_str(), response.size(), 0);
    return true;

}

void Connection::process_ws_frame() {
    update_heartbeat();  

    // WS recv

    while (read_buffer_.size() >= 2) {

        unsigned char* data = (unsigned char*)read_buffer_.data();
        size_t pos = 0;
        
        // FIN + Opcode
        uint8_t opcode = data[0] & 0x0F;

        if (opcode == 0x8) { 
            handle_close(); 
            return; 
        } // 关闭帧

        if (opcode != 0x1 && opcode != 0x2) {
            break; 
        }    // 只处理文本/二进制
        
        pos++;

        bool masked = data[1] & 0x80;
        uint64_t payload_len = data[1] & 0x7F;

        pos++;
        
        if (payload_len == 126) { 
            payload_len = (data[2]<<8)|data[3]; 
            pos += 2; 
        }
        else if (payload_len == 127) { 
            pos += 8; payload_len = ((uint64_t)data[2]<<56)|((uint64_t)data[3]<<48)|((uint64_t)data[4]<<40)|((uint64_t)data[5]<<32)|((uint64_t)data[6]<<24)|(data[7]<<16)|(data[8]<<8)|data[9]; 
        }
        
        unsigned char mask[4] = {0};

        if (masked) { 
            memcpy(mask, data+pos, 4); 
            pos += 4; 
        }
        
        if (read_buffer_.size() < pos + payload_len) {
            break;
        }
        
        // WS frame

        std::string payload(payload_len, 0);

        for (uint64_t i = 0; i < payload_len; i++){
            payload[i] = data[pos+i] ^ mask[i%4];
        }

        read_buffer_ = read_buffer_.substr(pos + payload_len);
        
        // 解析 WebSocket 消息体，转换成 MsgHeader
        if (payload.size() >= sizeof(MsgHeader)) {
            MsgHeader hdr;

            memcpy(&hdr, payload.data(), sizeof(MsgHeader));

            // 校验包长合法性
            if (hdr.length < sizeof(MsgHeader) || hdr.length > MAX_PACKET_SIZE) {
                handle_close();
                return;
            }

            if (payload.size() >= hdr.length) {

                std::string body_data = payload.substr(sizeof(MsgHeader), hdr.length - sizeof(MsgHeader));
                Buffer body(body_data);

                server_->dispatch(shared_from_this(), hdr, body);
            }
        }
    }
}

void Connection::send_ws(const std::string& data) {

    std::string frame;
    frame += (char)0x82; // FIN + 二进制帧

    if (data.size() <= 125) {
        frame += (char)data.size();
    }
    else if (data.size() <= 65535) {
        frame += (char)126;
        frame += (char)(data.size() >> 8);
        frame += (char)(data.size() & 0xFF);
    }
    else {
        frame += (char)127;
        uint64_t len = data.size();
        for (int i = 7; i >= 0; --i) {
            frame += (char)(len >> (i * 8));
        }
    }

    frame += data;

    std::lock_guard<std::mutex> lock(write_mutex_);
    write_buffer_ += frame;

    if (!writing_) {
        writing_ = true;
        reactor_->modify_event(fd_, EPOLLIN | EPOLLOUT);
    }


}

void Connection::send_file_response(const std::string& header, const std::string& file_path, uint64_t file_size) {
    int file_fd = open(file_path.c_str(), O_RDONLY);
    if (file_fd < 0) {
        LOG_ERROR("send_file: open failed path={} err={}", file_path, strerror(errno));
        return;
    }

    // mmap 映射文件，避免拷贝到用户态缓冲区
    void* mapped = mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, file_fd, 0);
    if (mapped == MAP_FAILED) {
        LOG_ERROR("send_file: mmap failed path={} err={}", file_path, strerror(errno));
        close(file_fd);
        return;
    }
    madvise(mapped, file_size, MADV_SEQUENTIAL);

    // 构建 WebSocket 帧头
    uint64_t total_payload = header.size() + file_size;
    std::string ws_header;
    ws_header += (char)0x82; // FIN + binary
    if (total_payload <= 125) {
        ws_header += (char)total_payload;
    } 
    else if (total_payload <= 65535) {
        ws_header += (char)126;
        ws_header += (char)(total_payload >> 8);
        ws_header += (char)(total_payload & 0xFF);
    } 
    else {
        ws_header += (char)127;
        for (int i = 7; i >= 0; --i)
            ws_header += (char)(total_payload >> (i * 8));
    }

    // 通过现有写缓冲区机制发送，mmap 内存直接 append，无额外拷贝
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        write_buffer_ += ws_header;
        write_buffer_ += header;
        write_buffer_.append(static_cast<const char*>(mapped), file_size);
        if (!writing_) {
            writing_ = true;
            reactor_->modify_event(fd_, EPOLLIN | EPOLLOUT);
        }
    }

    munmap(mapped, file_size);
    close(file_fd);
    LOG_INFO("send_file: sent path={} size={}", file_path, file_size);
}

void Connection::process_packet() {
    // process_packet debug
    
    while (read_buffer_.size() >= sizeof(MsgHeader)) {

        MsgHeader hdr;

        if (!MessageHelper::unpack_header(read_buffer_, hdr)) {
            break;
        }

        if (read_buffer_.size() < hdr.length) {
            break;
        }

        std::string body_data = read_buffer_.substr(sizeof(MsgHeader), hdr.length - sizeof(MsgHeader));

        read_buffer_ = read_buffer_.substr(hdr.length);
        
        Buffer body(body_data);
        server_->dispatch(shared_from_this(), hdr, body);
    }
}

} // namespace chat