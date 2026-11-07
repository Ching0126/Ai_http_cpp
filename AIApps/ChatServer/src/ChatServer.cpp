#include "ChatServer.h"
#include "handlers/StaticFileHandler.h"
#include "middleware/RequestLogMiddleware.h"
#include "middleware/CorsMiddleware.h"

#include <cstdlib>
#include <memory>
#include <mutex>

//static const std::string CHAT_RESOURCE_DIR= "${CMAKE_CURRENT_SOURCE_DIR}/AIApps/ChatServer/resource";

ChatServer::ChatServer(
    muduo::net::EventLoop* loop,
    unsigned short port
)
    :httpServer_(loop,port){
    
    httpServer_.addMiddleware(std::make_shared<RequestLogMiddleware>());//每一次请求都要经过它
    
    const char* corsOrigin =std::getenv("CORS_ALLOWED_ORIGIN");

    httpServer_.addMiddleware(std::make_shared<CorsMiddleware>(corsOrigin ? corsOrigin : ""));//不配置，默认不启动跨域

    const std::string resources = CHAT_RESOURCE_DIR;

            const StaticFileHandler page(
                resources + "/index.html",
                "text/html; charset=utf-8"
            );

            const StaticFileHandler stylesheet(
                resources + "/style.css",
                "text/css; charset=utf-8"
            );
    httpServer_.addRoute(
        "GET",
        "/",
        [page](const HttpRequest& request) {
            return page.handle(request);
        }         
    );

    httpServer_.addRoute(
    "GET",
    "/style.css",
    [stylesheet](const HttpRequest& request) {
        return stylesheet.handle(request);
    }
);
    
    // 注册用户。
    httpServer_.addRoute(
        "POST",
        "/register",
        [this](const HttpRequest& request) {
            std::lock_guard<std::mutex> lock(authMutex_);
            return authHandler_.registerUser(request);
        }
    );

    // 登录。
    httpServer_.addRoute(
        "POST",
        "/login",
        [this](const HttpRequest& request) {
            std::lock_guard<std::mutex> lock(authMutex_);
            return authHandler_.login(request);
        }
    );

    // 退出登录。
    httpServer_.addRoute(
        "POST",
        "/logout",
        [this](const HttpRequest& request) {
            std::lock_guard<std::mutex> lock(authMutex_);
            return authHandler_.logout(request);
        }
    );

    // 查询当前登录身份。
    httpServer_.addRoute(
        "GET",
        "/auth/me",
        [this](const HttpRequest& request) {
            std::lock_guard<std::mutex> lock(authMutex_);
            return authHandler_.me(request);
        }
    );

     // 创建会话。
    httpServer_.addRoute(
        "POST",
        "/chat/sessions",
        [this](const HttpRequest& request) {
            return guarded(request,&ChatHandler::createSession);
        }
    );

    // 查询会话列表。
    httpServer_.addRoute(
        "GET",
        "/chat/sessions",
        [this](const HttpRequest& request) {
            //return chatHandler_.listSessions(request);
            return guarded(request,&ChatHandler::listSessions);
        }
    );

    //注册聊天接口，含有请求正文，默认post请求
    httpServer_.addRoute(
        "POST",
        "/chat/send",
        [this](const HttpRequest& request) {
            // return chatHandler_.handle(request);
            return guarded(request,&ChatHandler::handle);
        }
    );
     // 查询指定会话聊天历史。
    httpServer_.addRoute(
        "POST",
        "/chat/history",
        [this](const HttpRequest& request) {
            // return chatHandler_.history(request);
            return guarded(request,&ChatHandler::history);
        }
    );

    // 清空指定聊天历史。
    httpServer_.addRoute(
        "POST",
        "/chat/clear",
        [this](const HttpRequest& request) {
            // return chatHandler_.clear(request);
            return guarded(request,&ChatHandler::clear);
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

HttpResponse ChatServer::guarded(
    const HttpRequest& request,
    ChatAction action
){
    std::string username;
    {
        std::lock_guard<std::mutex> lock(authMutex_);
        if(!authHandler_.authenticatedUser(request,username)){
            return HttpResponse{
                401,
                "Unauthorized",
                R"({"error":"unauthorized"})"
            };
        }
    }
    std::lock_guard<std::mutex> lock(chatMutex_);
    try {
        return (chatHandler_.*action)(request, username);
    } catch (const ChatStorageError&) {
        return HttpResponse(
            503,
            "Service Unavailable",
            R"({"error":"chat storage unavailable"})"
        );
    }
}
