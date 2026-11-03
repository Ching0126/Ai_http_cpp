#pragma once

#include "handlers/ChatHandler.h"
#include "http/HttpServer.h"

class ChatServer{
public:
    ChatServer(
        muduo::net::EventLoop* loop,
        unsigned short port
    );

    void start();

private:
    ChatHandler chatHandler_;//确保多次请求可以访问同一份历史数据。
    HttpServer httpServer_;
};
