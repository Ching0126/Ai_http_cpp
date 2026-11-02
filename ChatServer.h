#pragma once

#include "HttpServer.h"

class ChatServer{
public:
    ChatServer(
        muduo::net::EventLoop* loop,
        unsigned short port
    );

    void start();

private:
    HttpServer httpServer_;
};
