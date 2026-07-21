#include "/home/lighning/codes/test/linux/聊天室/include/server/connection.h"
#include "/home/lighning/codes/test/linux/聊天室/include/server/reactor.h"
#include "/home/lighning/codes/test/linux/聊天室/include/server/server.h"
#include "/home/lighning/codes/test/linux/聊天室/include/server/logger.h"
#include <unistd.h>
#include <sys/socket.h>

namespace chat {

Connection::Connection(int fd, SubReactor* reactor, Server* server)
    : fd_(fd), reactor_(reactor), server_(server) {
    last_heartbeat_ = time(nullptr);
}

Connection::~Connection() { close(fd_); }

void Connection::handle_read() {
    char buf[65536];
    while (true) {
        int n = recv(fd_, buf, sizeof(buf), 0);
        printf("DEBUG recv fd=%d n=%d\n", fd_, n); 
        if (n > 0) {
            read_buffer_.append(buf, n);
            printf("DEBUG buffer size=%zu\n", read_buffer_.size()); 
        } else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
            handle_close();
            return;
        } else {
            break;
        }
    }
    process_packet();
}

void Connection::handle_write() { do_write(); }

void Connection::handle_close() {
    reactor_->remove_connection(fd_);
    server_->on_connection_close(fd_);
}

void Connection::send_raw(const std::string& data) {
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
        if (n > 0) write_buffer_ = write_buffer_.substr(n);
        else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) { writing_ = false; return; }
        else break;
    }
    if (write_buffer_.empty()) {
        writing_ = false;
        reactor_->modify_event(fd_, EPOLLIN);
    }
}

void Connection::process_packet() {
    printf("DEBUG process_packet buffer=%zu\n", read_buffer_.size());
    while (read_buffer_.size() >= sizeof(MsgHeader)) {
        MsgHeader hdr;
        if (!MessageHelper::unpack_header(read_buffer_, hdr)) break;
        if (read_buffer_.size() < hdr.length) break;

        std::string body_data = read_buffer_.substr(sizeof(MsgHeader), hdr.length - sizeof(MsgHeader));
        read_buffer_ = read_buffer_.substr(hdr.length);
        Buffer body(body_data);
        server_->dispatch(shared_from_this(), hdr, body);
    }
}

} // namespace chat