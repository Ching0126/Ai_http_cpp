#pragma once 

#include <string>
//将聊天时间发布到RabbitMQ
class ChatEventPublisher{
public:
    static bool enabled();//检查是否启用事件发布

    static void publish(const std::string& eventJson);//发布一条聊天事件
};