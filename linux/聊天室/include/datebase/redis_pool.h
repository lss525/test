
#define CHAT_REDIS_POOL_H

#include <hiredis/hiredis.h>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace chat {

class RedisPool {
public:
    RedisPool(const std::string& host, int port, const std::string& password,
              int db = 0, int pool_size = 10);
    ~RedisPool();
    redisContext* acquire();
    void release(redisContext* conn);
    bool set(const std::string& key, const std::string& value, int expire = 0);
    std::string get(const std::string& key);
    bool del(const std::string& key);
private:
    std::string host_, password_;
    int port_, db_, pool_size_;
    std::queue<redisContext*> pool_;
    std::mutex mutex_;
    std::condition_variable cv_;
    redisContext* create_connection();
};

} // namespace chat
