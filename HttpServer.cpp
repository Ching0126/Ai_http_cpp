#include "HttpServer.h"
#include "HttpContext.h"

#include <boost/any.hpp>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <functional>
#include <utility>

HttpServer::HttpServer(
    muduo::net::EventLoop* loop,
    unsigned short port
):server_(
    loop,
    muduo::net::InetAddress("127.0.0.1",port),
    "AiHttp"
){
     server_.setConnectionCallback(
        std::bind(
            &HttpServer::onConnection,
            this,
            std::placeholders::_1
        )
    );

    server_.setMessageCallback(
        std::bind(
            &HttpServer::onMessage,
            this,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3
        )
    );
}

void HttpServer::addRoute(
    const std::string& method,
    const std::string& path,
    Router::Handler handler
){
    router_.addRoute(
        method,
        path,
        std::move(handler)
    );
}

void HttpServer::start() {
    server_.start();
}

void HttpServer::onConnection(
    const muduo::net::TcpConnectionPtr& conn
) {
    if (conn->connected()) {
        conn->setContext(HttpContext{});
    }
}

void HttpServer::onMessage(
    const muduo::net::TcpConnectionPtr& conn,
    muduo::net::Buffer* buffer,
    muduo::Timestamp
) {
    if (!conn->connected()) {
        return;
    }

    auto* context = boost::any_cast<HttpContext>(
        conn->getMutableContext()
    );

    if (!context) {
        sendResponse(
            conn,
            HttpResponse(
                500,
                "Internal Server Error",
                R"({"error":"missing context"})"
            )
        );

        return;
    }

    const auto result = context->parse(buffer);

    if (result == HttpContext::Result::NeedMore) {
        return;
    }

    if (result == HttpContext::Result::Error) {
        sendResponse(
            conn,
            HttpResponse(
                context->errorStatus(),
                context->errorReason(),
                context->errorBody()
            )
        );

        return;
    }

    // 请求解析完成后交给路由器。
    const HttpResponse response =
        router_.route(context->request());

    sendResponse(conn, response);
}

void HttpServer::sendResponse(
    const muduo::net::TcpConnectionPtr& conn,
    const HttpResponse& response
) {
    conn->send(response.serialize());

    // 本批仍然是一条连接只处理一次请求。
    conn->shutdown();
}