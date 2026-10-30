#pragma once

#include "HttpRequest.h"
#include "HttpResponse.h"

#include <muduo/net/TcpServer.h>
#include <string>

class ChatServer{
public:
    ChatServer(
        muduo::net::EventLoop* loop,
        unsigned short port
    );

    void start();

private:

    void onConnection(
        const muduo::net::TcpConnectionPtr& conn
    );

    void onMessage(
        const muduo::net::TcpConnectionPtr& conn,
        muduo::net::Buffer* buffer,
        muduo::Timestamp receiveTime
    );

    HttpResponse handleRequest(
        const HttpRequest& request
    );

    void sendResponse(
        const muduo::net::TcpConnectionPtr& conn,
        const HttpResponse& response
    );

    muduo::net::TcpServer server_;
};
