#include "/home/lighning/codes/test/linux/聊天室/include/server/reactor.h"
#include "/home/lighning/codes/test/linux/聊天室/include/server/connection.h"
#include "/home/lighning/codes/test/linux/聊天室/include/server/server.h"
#include "/home/lighning/codes/test/linux/聊天室/include/server/logger.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>

namespace chat {

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

SubReactor::SubReactor(int id) : id_(id) {
    epoll_fd_ = epoll_create1(0);
}

SubReactor::~SubReactor() {
    stop();
    close(epoll_fd_);
}

void SubReactor::run() {
    running_ = true;
    thread_ = std::thread(&SubReactor::event_loop, this);
}

void SubReactor::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void SubReactor::add_connection(std::shared_ptr<Connection> conn) {
    epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = conn->get_fd();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        connections_[conn->get_fd()] = conn;
    }
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, conn->get_fd(), &ev);
    set_nonblocking(conn->get_fd());
}

void SubReactor::remove_connection(int fd) {
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    std::lock_guard<std::mutex> lock(mutex_);
    connections_.erase(fd);
}

void SubReactor::modify_event(int fd, uint32_t events) {
    epoll_event ev;
    ev.events = events | EPOLLET;
    ev.data.fd = fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
}

void SubReactor::event_loop() {
    epoll_event events[1024];
    while (running_) {
        int n = epoll_wait(epoll_fd_, events, 1024, 100);
        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            std::shared_ptr<Connection> conn;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = connections_.find(fd);
                if (it != connections_.end()) conn = it->second;
            }
            if (!conn) continue;
            if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                conn->handle_close();
            } else {
                if (events[i].events & EPOLLIN)  conn->handle_read();
                if (events[i].events & EPOLLOUT) conn->handle_write();
            }
        }
    }
}

MainReactor::MainReactor(Server* server, int port, const std::string& address)
    : server_(server) {
    epoll_fd_ = epoll_create1(0);
    setup_listen_socket(port, address);
}

MainReactor::~MainReactor() {
    stop();
    close(listen_fd_);
    close(epoll_fd_);
}

void MainReactor::setup_listen_socket(int port, const std::string& address) {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, address.c_str(), &addr.sin_addr);

    bind(listen_fd_, (sockaddr*)&addr, sizeof(addr));
    listen(listen_fd_, SOMAXCONN);
    set_nonblocking(listen_fd_);

    epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listen_fd_;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev);
}

void MainReactor::run() {
    running_ = true;
    thread_ = std::thread(&MainReactor::event_loop, this);
}

void MainReactor::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void MainReactor::event_loop() {
    epoll_event events[64];
    while (running_) {
        int n = epoll_wait(epoll_fd_, events, 64, 100);
        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd == listen_fd_) {
                sockaddr_in client_addr;
                socklen_t len = sizeof(client_addr);
                int client_fd = accept4(listen_fd_, (sockaddr*)&client_addr, &len, SOCK_NONBLOCK);
                if (client_fd > 0) server_->on_new_connection(client_fd);
            }
        }
        server_->check_heartbeat();
    }
}

WorkerPool::WorkerPool(int num_threads) { threads_.reserve(num_threads); }
WorkerPool::~WorkerPool() { stop(); }

void WorkerPool::start() {
    running_ = true;
    for (size_t i = 0; i < threads_.capacity(); ++i)
        threads_.emplace_back(&WorkerPool::worker_loop, this);
}

void WorkerPool::stop() {
    running_ = false;
    cv_.notify_all();
    for (auto& t : threads_)
        if (t.joinable()) t.join();
}

void WorkerPool::submit(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_.push(std::move(task));
    }
    cv_.notify_one();
}

void WorkerPool::worker_loop() {
    while (running_) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return !tasks_.empty() || !running_; });
            if (!running_ && tasks_.empty()) return;
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task();
    }
}

HeartbeatTimer::HeartbeatTimer(int timeout_seconds) : timeout_seconds_(timeout_seconds) {}

void HeartbeatTimer::update(int fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    heartbeat_times_[fd] = time(nullptr);
}

void HeartbeatTimer::remove(int fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    heartbeat_times_.erase(fd);
}

std::vector<int> HeartbeatTimer::check_timeout() {
    std::vector<int> fds;
    time_t now = time(nullptr);
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = heartbeat_times_.begin(); it != heartbeat_times_.end(); ) {
        if (now - it->second > timeout_seconds_) {
            fds.push_back(it->first);
            it = heartbeat_times_.erase(it);
        } else { ++it; }
    }
    return fds;
}

} // namespace chat