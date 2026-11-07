#pragma once

#include <middleware/MiddlewareChain.h>
#include "router/Router.h"
#include <muduo/net/TcpServer.h>
#include <mutex>

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
    
    void addMiddleware(std::shared_ptr<Middleware> middleware);

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

    Router router_;
    MiddlewareChain middlewareChain_;

    // 最后声明，使 server_ 在析构时先于上面几个成员销毁。
    muduo::net::TcpServer server_;

};