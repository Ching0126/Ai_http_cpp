#include "storage/DbConnectionPool.h"

#include <mysql_driver.h>
#include <cppconn/connection.h>

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::string envOr(const char* name,const char* fallback) {
    const char* value = std::getenv(name);

    return value && *value ? value : fallback;
}

std::size_t readPoolSize() {
    const auto text = envOr("AI_DB_POOL_SIZE","3");//这是string 

    if (text.empty() || text.size() > 2) {//里面由几个数字
        throw std::invalid_argument("invalid AI_DB_POOL_SIZE");
    }

    unsigned number = 0;

    for (char c : text) {
        if (c < '0' || c > '9') {
            throw std::invalid_argument("invalid AI_DB_POOL_SIZE");
        }

        number = number * 10+ static_cast<unsigned>(c - '0');
    }

    if (number < 1 || number > 32) {
        throw std::invalid_argument("invalid AI_DB_POOL_SIZE");
    }

    return number;
}

}//namespace

//连接池的内部共享状态
struct DbConnectionPool::State {
    State()
        : host(envOr("AI_DB_HOST","tcp://127.0.0.1:3306")),
          user(envOr("AI_DB_USER","ai_http_user")),
          password(envOr("AI_DB_PASSWORD","")),
          database(envOr("AI_DB_NAME","ai_http_learning")),
          limit(readPoolSize()),//最大连接数
          driver(sql::mysql::get_mysql_driver_instance()) {//构造数据库实例
        // 提前分配空间，归还连接时不需要扩容。
        idle.reserve(limit);//预留空间
    }
    //归还连接
     void release(sql::Connection* raw) noexcept {
        std::unique_ptr<sql::Connection> conn(raw);

        bool reusable = true;

        try {
            if (!conn->getAutoCommit()) {//当前数据库没有开启自动提交
                conn->rollback();//回滚未提交的事物
                conn->setAutoCommit(true);//自动提交
            }
        } catch (...) {
            reusable = false;
        }

        {
            std::lock_guard<std::mutex> lock(mutex);

            if (reusable) {
                idle.push_back(std::move(conn));
            } else {
                --total;
            }
        }

        // 通知一个正在等待连接的线程。
        available.notify_one();
    }

    std::string host;
    std::string user;
    std::string password;
    std::string database;

    std::size_t limit;

    sql::mysql::MySQL_Driver* driver;

    // 保护 idle 和 total。
    std::mutex mutex;

    // 串行使用 driver 创建连接。
    std::mutex connectMutex;

    // 池满时，借用者在这里等待。
    std::condition_variable available;

    // 当前空闲的连接。
    std::vector<std::unique_ptr<sql::Connection>> idle;

    // 总数 = 空闲 + 已借出 + 正在创建。
    std::size_t total = 0;
};
/**
 * 构造连接池，创建共享状态。
 *
 * 这里只读取配置，不创建数据库连接。
 */
DbConnectionPool::DbConnectionPool()
    : state_(std::make_shared<State>()) {
}

/**
 * 获取进程内唯一的连接池。
 */
DbConnectionPool& DbConnectionPool::instance() {
    static DbConnectionPool pool;
    return pool;
}

/**
 * 借出连接。
 *
 * 优先复用空闲连接；空闲连接不存在且未达到上限时，
 * 创建新连接；达到上限则等待。
 */
std::shared_ptr<sql::Connection>
DbConnectionPool::acquire() {
    auto state = state_;

    const auto deadline = std::chrono::steady_clock::now()+ std::chrono::seconds(1);//超时等待1s

    for (;;) {//尝试获取里连接
        std::unique_ptr<sql::Connection> conn;

        bool create = false;

        {
            std::unique_lock<std::mutex> lock(state->mutex);

            // 有空闲连接，或者还可以创建新连接时，继续执行。
            if (!state->available.wait_until(lock,deadline,[&] {return !state->idle.empty()//有连接，或者现存连接数没达到最大上限
                                                                || state->total < state->limit;})) {
                throw std::runtime_error("database pool wait timeout");
            }

            if (!state->idle.empty()) {//存在连接
                // 取出一个空闲连接。
                conn = std::move(state->idle.back());

                state->idle.pop_back();
            } else {//没有空闲连接
                // 先占用一个名额，再解锁创建连接。
                ++state->total;
                create = true;
            }
        }

        if (create) {//需要新连接
            try {
                std::lock_guard<std::mutex> lock(
                    state->connectMutex
                );

                conn.reset(
                    state->driver->connect(state->host,state->user,state->password));

                conn->setSchema(state->database);
            } catch (...) {
                // 创建失败，归还刚才占用的名额。
                {
                    std::lock_guard<std::mutex> lock(state->mutex);

                    --state->total;
                }

                state->available.notify_one();

                throw;
            }
        } else {//如果拿到就链接 检查是否有效
            // 已有连接可能因为数据库重启等原因失效。
            bool valid = false;

            try {
                valid = conn->isValid();
            } catch (...) {
                valid = false;
            }

            if (!valid) {//存在失效连接
                // 丢弃失效连接，并释放连接名额。
                conn.reset();

                {
                    std::lock_guard<std::mutex> lock(state->mutex);

                    --state->total;
                }

                state->available.notify_one();

                // 回到循环，重新尝试获取连接。
                continue;
            }
        }

        // 自定义删除器负责归还连接。
        return std::shared_ptr<sql::Connection>(conn.release(),//放弃指针的所有权
            [state](
                sql::Connection* connection) noexcept {
                    state->release(connection);//调用连接池，归还连接
                }
        );
    }
}
