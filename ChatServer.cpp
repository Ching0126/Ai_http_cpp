#include "ChatServer.h"
#include "ChatHandler.h"


ChatServer::ChatServer(
    muduo::net::EventLoop* loop,
    unsigned short port
)
    :httpServer_(loop,port){
    httpServer_.addRoute(
        "POST",
        "/chat/send",
        ChatHandler::handle
    );

    // 注册健康检查接口。
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
