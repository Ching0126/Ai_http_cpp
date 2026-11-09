#pragma once 

#include "ai/ChatStrategy.h"

#include <memory>
#include <string>

class ChatStrategyFactory{
public:
    static std::unique_ptr<ChatStrategy> create(const std::string& mode);
};