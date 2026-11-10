#pragma once 

#include "ai/ChatMessage.h"
#include "tools/ToolRegistry.h"

#include <stdexcept>
#include <string>
#include <vector>

class AIConfigError:public std::runtime_error{
public:
    using std::runtime_error::runtime_error;
};

class AIRequestError:public std::runtime_error{
public:
    using std::runtime_error::runtime_error;
};

class  ChatStrategy{
public:
    virtual ~ChatStrategy()=default;

    virtual std::string ask(const std::vector<ChatMessage>& history,const ToolRegistry& tools)const = 0;
};

class  MockChatStrategy final : public ChatStrategy{
public:
    std::string ask(const std::vector<ChatMessage>& history,const ToolRegistry& tools) const override;
};

class DeepSeekChatStrategy final : public ChatStrategy{
public:
    DeepSeekChatStrategy(std::string key,std::string url,std::string model);

    std::string ask(const std::vector<ChatMessage>& history,const ToolRegistry& tools)const override;
private:
    std::string key_;
    std::string url_;
    std::string model_;
};