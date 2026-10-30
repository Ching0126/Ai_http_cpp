#pragma once

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

    void onMessage(
        const muduo::net::TcpConnectionPtr& conn,
        muduo::net::Buffer* buffer,
        muduo::Timestamp receiveTime
    );

    void handleChat(
        const muduo::net::TcpConnectionPtr& conn,
        const std::string& body
    );

    void reply(
        const muduo::net::TcpConnectionPtr& conn,
        int status,
        const std::string& reason,
        const std::string& body
    );

    muduo::net::TcpServer server_;
};
