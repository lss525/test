#ifndef CHAT_REACTOR_H
#define CHAT_REACTOR_H

#include <sys/epoll.h>
#include <atomic>
#include <thread>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <functional>
#include <ctime>
#include <vector>

namespace chat {

class Connection;
class Server;

class SubReactor {
public:
    explicit SubReactor(int id);
    ~SubReactor();
    void run();
    void stop();
    void add_connection(std::shared_ptr<Connection> conn);
    void remove_connection(int fd);
    void modify_event(int fd, uint32_t events);
private:
    int id_;
    int epoll_fd_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    std::map<int, std::shared_ptr<Connection>> connections_;
    std::mutex mutex_;
    void event_loop();
};

class MainReactor {
public:
    MainReactor(Server* server, int port, const std::string& address);
    ~MainReactor();
    void run();
    void stop();
private:
    Server* server_;
    int listen_fd_;
    int epoll_fd_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    void setup_listen_socket(int port, const std::string& address);
    void event_loop();
};

class WorkerPool {
public:
    explicit WorkerPool(int num_threads = 4);
    ~WorkerPool();
    void start();
    void stop();
    void submit(std::function<void()> task);
private:
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_{false};
    void worker_loop();
};

class HeartbeatTimer {
public:
    explicit HeartbeatTimer(int timeout_seconds = 60);
    void update(int fd);
    void remove(int fd);
    std::vector<int> check_timeout();
private:
    int timeout_seconds_;
    std::map<int, time_t> heartbeat_times_;
    std::mutex mutex_;
};

} // namespace chat

#endif