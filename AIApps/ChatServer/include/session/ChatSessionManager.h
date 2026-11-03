#pragma once

#include "ai/ChatMessage.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>
//一个连天对话的id和消息列表
struct ChatSession{
    std::string id;
    std::vector<ChatMessage> messages;
};
//聊天会话管理器
class ChatSessionManager{
public:
    std::string create();

    ChatSession* find(const std::string& id);

    std::vector<std::string> list()const;
private:
    std::uint64_t nextId_ = 1;
    std::map<std::string,ChatSession> sessions_;
};