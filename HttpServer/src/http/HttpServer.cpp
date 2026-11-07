#include "http/HttpServer.h"
#include "http/HttpContext.h"

#include <boost/any.hpp>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <functional>
#include <utility>
#include <exception>

HttpServer::HttpServer(
    muduo::net::EventLoop* loop,
    unsigned short port
):server_(
    loop,
    muduo::net::InetAddress("127.0.0.1",port),
    "AiHttp"
){
     server_.setThreadNum(2);
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

void HttpServer::addMiddleware(std::shared_ptr<Middleware> middleware){
    middlewareChain_.add(std::move(middleware));
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
    //const HttpResponse response = router_.route(context->request());

    
    // 如果业务处理抛出未处理的标准异常，默认返回 500。
    HttpResponse response(
        500,
        "Internal Server Error",
        R"({"error":"request failed"})"
    );

    try {

        response = middlewareChain_.process(
            context->request(),
            [this](const HttpRequest& request) {
                return router_.route(request);
            }
        );
    } catch (const std::exception&) {
        // 保留上面构造的 500 响应。
    }

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