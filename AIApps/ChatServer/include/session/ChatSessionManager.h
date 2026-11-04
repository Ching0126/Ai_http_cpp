#pragma once

#include "ai/ChatMessage.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>
//一个连天对话的id和消息列表
struct ChatSession{
    std::string id;
    std::string owner;//会话的用户名
    std::vector<ChatMessage> messages;
};
//聊天会话管理器
class ChatSessionManager{
public:
    std::string create(const std::string& owner);

    ChatSession* find(const std::string& id,const std::string& owner);

    std::vector<std::string> list(const std::string& owner)const;
private:
    std::uint64_t nextId_ = 1;
    
    std::map<std::string,ChatSession> sessions_;
};