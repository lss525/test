#include "/home/lighning/codes/test/linux/聊天室/include/datebase/mysql_pool.h"
#include <stdexcept>

namespace chat {

MySQLPool::MySQLPool(const std::string& host, int port, const std::string& user,
                     const std::string& password, const std::string& database, int pool_size)
    : host_(host), port_(port), user_(user), password_(password), database_(database), pool_size_(pool_size) {
    mysql_library_init(0, nullptr, nullptr);
    for (int i = 0; i < pool_size_; ++i) pool_.push(create_connection());
}

MySQLPool::~MySQLPool() {
    while (!pool_.empty()) { mysql_close(pool_.front()); pool_.pop(); }
    mysql_library_end();
}

MYSQL* MySQLPool::create_connection() {
    MYSQL* conn = mysql_init(nullptr);
    if (!mysql_real_connect(conn, host_.c_str(), user_.c_str(), password_.c_str(),
                            database_.c_str(), port_, nullptr, 0))
        throw std::runtime_error(mysql_error(conn));
    return conn;
}

MYSQL* MySQLPool::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return !pool_.empty(); });
    MYSQL* conn = pool_.front(); pool_.pop();
    return conn;
}

void MySQLPool::release(MYSQL* conn) {
    std::lock_guard<std::mutex> lock(mutex_);
    pool_.push(conn);
    cv_.notify_one();
}

} // namespace chat