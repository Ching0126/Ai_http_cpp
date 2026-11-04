#pragma once

#include "handlers/ChatHandler.h"
#include "http/HttpServer.h"
#include "auth/AuthHandler.h"

class ChatServer{
public:
    ChatServer(
        muduo::net::EventLoop* loop,
        unsigned short port
    );

    void start();

private:
     /**
     * 聊天处理函数的成员函数指针类型。
     *
     * 这些函数都接收：
     * - HTTP 请求
     * - 已验证的用户名
     *
     * 并返回 HTTP 响应。
     */
    using ChatAction = HttpResponse(ChatHandler::*)(//成员函数指针类型
        const HttpRequest&,
        const std::string&
    );
    
    //  检查登录状态，成功后执行指定聊天处理函数。
    HttpResponse guarded(
        const HttpRequest& request,
        ChatAction action
    );

    AuthHandler authHandler_;
    ChatHandler chatHandler_;//确保多次请求可以访问同一份历史数据。
    HttpServer httpServer_;
};
