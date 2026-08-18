#include "../include/datebase/redis_pool.h"
#include <stdexcept>
#include <cstring>
#include <chrono>

namespace chat {

RedisPool::RedisPool(const std::string& host, int port, const std::string& password,
                     int db, int pool_size)
    : host_(host), port_(port), password_(password), db_(db), pool_size_(pool_size) {
    for (int i = 0; i < pool_size_; ++i) pool_.push(create_connection());
}

RedisPool::~RedisPool() {
    while (!pool_.empty()) { redisFree(pool_.front()); pool_.pop(); }
}

redisContext* RedisPool::create_connection() {
    redisContext* c = redisConnect(host_.c_str(), port_);
    if (!c || c->err) throw std::runtime_error("Redis connect failed");
    if (!password_.empty()) {
        auto* r = (redisReply*)redisCommand(c, "AUTH %s", password_.c_str());
        freeReplyObject(r);
    }
    if (db_ > 0) {
        auto* r = (redisReply*)redisCommand(c, "SELECT %d", db_);
        freeReplyObject(r);
    }
    return c;
}

redisContext* RedisPool::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    // 带超时等待，防止连接池耗尽时 worker 无限阻塞
    if (!cv_.wait_for(lock, std::chrono::seconds(5),
                      [this] { return !pool_.empty(); })) {
        throw std::runtime_error("RedisPool: acquire timeout");
    }
    redisContext* c = pool_.front(); pool_.pop();
    return c;
}

void RedisPool::release(redisContext* c) {
    std::lock_guard<std::mutex> lock(mutex_);
    pool_.push(c);
    cv_.notify_one();
}

bool RedisPool::set(const std::string& key, const std::string& value, int expire) {
    auto* c = acquire();
    bool ok = false;
    if (expire > 0) {
        auto* r = (redisReply*)redisCommand(c, "SETEX %s %d %s", key.c_str(), expire, value.c_str());
        ok = r && r->str && strcmp(r->str, "OK") == 0;
        freeReplyObject(r);
    } else {
        auto* r = (redisReply*)redisCommand(c, "SET %s %s", key.c_str(), value.c_str());
        ok = r && r->str && strcmp(r->str, "OK") == 0;
        freeReplyObject(r);
    }
    release(c);
    return ok;
}

std::string RedisPool::get(const std::string& key) {
    auto* c = acquire();
    auto* r = (redisReply*)redisCommand(c, "GET %s", key.c_str());
    std::string v = (r && r->str) ? r->str : "";
    freeReplyObject(r);
    release(c);
    return v;
}

bool RedisPool::del(const std::string& key) {
    auto* c = acquire();
    auto* r = (redisReply*)redisCommand(c, "DEL %s", key.c_str());
    bool ok = r && r->integer > 0;
    freeReplyObject(r);
    release(c);
    return ok;
}

} // namespace chat