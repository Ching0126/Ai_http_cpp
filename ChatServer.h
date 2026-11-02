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
    //建立连接
    void onConnection(
        const muduo::net::TcpConnectionPtr& conn
    );
    //处理请求
    void onMessage(
        const muduo::net::TcpConnectionPtr& conn,
        muduo::net::Buffer* buffer,
        muduo::Timestamp receiveTime
    );
    //根据请求生成业务响应
    HttpResponse handleRequest(
        const HttpRequest& request
    );
    //发送响应，关闭连接
    void sendResponse(
        const muduo::net::TcpConnectionPtr& conn,
        const HttpResponse& response
    );

    muduo::net::TcpServer server_;
};
