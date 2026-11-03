#include "ChatServer.h"


ChatServer::ChatServer(
    muduo::net::EventLoop* loop,
    unsigned short port
)
    :httpServer_(loop,port){
     // 创建会话。
    httpServer_.addRoute(
        "POST",
        "/chat/sessions",
        [this](const HttpRequest& request) {
            return chatHandler_.createSession(request);
        }
    );

    // 查询会话列表。
    httpServer_.addRoute(
        "GET",
        "/chat/sessions",
        [this](const HttpRequest& request) {
            return chatHandler_.listSessions(request);
        }
    );

    //注册聊天接口，含有请求正文，默认post请求
    httpServer_.addRoute(
        "POST",
        "/chat/send",
        [this](const HttpRequest& request) {
            return chatHandler_.handle(request);
        }
    );
     // 查询指定会话聊天历史。
    httpServer_.addRoute(
        "POST",
        "/chat/history",
        [this](const HttpRequest& request) {
            return chatHandler_.history(request);
        }
    );

    // 清空指定聊天历史。
    httpServer_.addRoute(
        "POST",
        "/chat/clear",
        [this](const HttpRequest& request) {
            return chatHandler_.clear(request);
        }
    );
    // 注册健康检查接口。没有请求正文，默认是get请求
    httpServer_.addRoute(
        "GET",
        "/health",
        [](const HttpRequest&) {
            return HttpResponse(
                200,
                "OK",
                R"({"status":"ok"})"
            );
        }
    );
}

void ChatServer::start(){
    httpServer_.start();
}
