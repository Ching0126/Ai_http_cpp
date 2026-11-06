#pragma once

#include "ai/ChatMessage.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <stdexcept>
//一个连天对话的id和消息列表
struct ChatSession{
    std::string id;
    std::string owner;//会话的用户名
    std::vector<ChatMessage> messages;
};

class ChatStorageError:public std::runtime_error{
public:
    using std::runtime_error::runtime_error;
};

//聊天会话管理器
class ChatSessionManager{
public:

    ChatSessionManager();

    std::string create(const std::string& owner);

    ChatSession* find(const std::string& id,const std::string& owner);

    std::vector<std::string> list(const std::string& owner)const;

    bool save(const std::string& id,const std::string& owner,const std::vector<ChatMessage>& messages);
private:

    bool mysqlMode_ =false;

    std::string host_;
    std::string user_;
    std::string password_;
    std::string database_;

    std::uint64_t nextId_ = 1;
    
    std::map<std::string,ChatSession> sessions_;
};