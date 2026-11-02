#include "ChatServer.h"
#include "ChatHandler.h"


ChatServer::ChatServer(
    muduo::net::EventLoop* loop,
    unsigned short port
)
    :httpServer_(loop,port){
    //注册聊天接口，含有请求正文，默认post请求
    httpServer_.addRoute(
        "POST",
        "/chat/send",
        ChatHandler::handle
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
