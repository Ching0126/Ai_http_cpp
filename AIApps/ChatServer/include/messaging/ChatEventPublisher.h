#pragma once 

#include <string>

class ChatEventPublisher{
public:
    static bool enabled();

    static void publish(const std::string& eventJson);
};