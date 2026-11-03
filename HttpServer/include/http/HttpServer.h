#pragma once

#include "router/Router.h"
#include <muduo/net/TcpServer.h>

class HttpServer{
public:
    HttpServer(
        muduo::net::EventLoop* loop,
        unsigned short port
    );

    void addRoute(
        const std::string& method,
        const std::string& path,
        Router::Handler handler
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

    void sendResponse(
        const muduo::net::TcpConnectionPtr& conn,
        const HttpResponse& response
    );

    muduo::net::TcpServer server_;
    Router router_;
};