#pragma once

#include <memory>

namespace sql{
class Connection;
}

//连接池
class DbConnectionPool{
public:
    static DbConnectionPool& instance();//获取实例
    
    //借连接
    std::shared_ptr<sql::Connection> acquire();

    DbConnectionPool(const DbConnectionPool&)=delete;
    DbConnectionPool& operator=(const DbConnectionPool&) = delete;
private:
    DbConnectionPool();

    struct State;//声明一个结构体

    std::shared_ptr<State> state_;
};