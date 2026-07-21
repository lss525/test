#ifndef CHAT_MYSQL_POOL_H
#define CHAT_MYSQL_POOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace chat {

class MySQLPool {
public:
    MySQLPool(const std::string& host, int port, const std::string& user,
              const std::string& password, const std::string& database, int pool_size = 20);
    ~MySQLPool();
    MYSQL* acquire();
    void release(MYSQL* conn);
private:
    std::string host_, user_, password_, database_;
    int port_, pool_size_;
    std::queue<MYSQL*> pool_;
    std::mutex mutex_;
    std::condition_variable cv_;
    MYSQL* create_connection();
};

} // namespace chat

#endif