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
    ChatHandler chatHandler_;
    HttpServer httpServer_;
};
