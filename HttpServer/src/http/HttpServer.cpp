#include "http/HttpServer.h"
#include "http/HttpContext.h"

#include <boost/any.hpp>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <functional>
#include <utility>
#include <exception>
#include <muduo/net/EventLoop.h>
// Muduo 的 2 个 I/O 线程
//     负责连接、收发数据、解析请求
//               ↓ 提交任务
// 你的 TaskExecutor 的 4 个业务线程
//     负责中间件、路由和业务处理
HttpServer::HttpServer(
    muduo::net::EventLoop* loop,
    unsigned short port
):executor_(4,32)//4个工作线程，32个排队任务
 ,server_(
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

HttpServer::~HttpServer() {
    executor_.stop();
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
     // 当前框架仍然是一个连接处理一个请求。
    // 请求已经完整，停止读取后续数据。
    conn->stopRead();

    // 请求属于连接上的 HttpContext。
    // 工作线程异步执行，所以这里复制一份。
    const HttpRequest request = context->request();

    const bool accepted = executor_.trySubmit(//提交任务
        [this, conn, request] {
            HttpResponse response(
                500,
                "Internal Server Error",
                R"({"error":"request failed"})"
            );

            try {
                response = middlewareChain_.process(//中间件处理+路由
                    request,
                    [this](const HttpRequest& req) {
                        return router_.route(req);
                    }
                );
            } catch (...) {
                // 保留默认的 500 响应。
            }

            const std::string data = response.serialize();

            // 回到连接所属的 I/O 线程发送响应。
            conn->getLoop()->queueInLoop(
                [conn, data] {
                    if (conn->connected()) {
                        conn->send(data);
                        conn->shutdown();
                    }
                }
            );
        }
    );

    if (!accepted) {
        // 等待队列已满或执行器正在停止。
        sendResponse(
            conn,
            HttpResponse(
                503,
                "Service Unavailable",
                R"({"error":"server busy"})"
            )
        );
    }
    
}

void HttpServer::sendResponse(
    const muduo::net::TcpConnectionPtr& conn,
    const HttpResponse& response
) {
    conn->send(response.serialize());

    // 本批仍然是一条连接只处理一次请求。
    conn->shutdown();
}