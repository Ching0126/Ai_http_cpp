#pragma once

#include <stdexcept>
#include <string>

class InvalidChatEvent:public std::runtime_error{
public:
using std::runtime_error::runtime_error;//继承构造函数
// 可以使用基类 std::runtime_error 的构造函数。
};
//校验并保存聊天事件
class ChatEventStore{
public:
    bool save(const std::string& body) const;
};