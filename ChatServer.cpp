#include "ChatServer.h"
#include "HttpContext.h"

#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include<boost/any.hpp>
#include <boost/json/src.hpp>
#include <functional>

ChatServer::ChatServer(
    muduo::net::EventLoop* loop,
    unsigned short port
)
    :server_(
        loop,
        muduo::net::InetAddress("127.0.0.1",port),
        "AiHttp"
    ){
    //将Muduo的数据到达时间绑定成员函数
    server_.setConnectionCallback(
        std::bind(
            &ChatServer::onConnection,//这个函数的三个参数
            this,
            std::placeholders::_1
        )
    );

    server_.setMessageCallback(
        std::bind(
            &ChatServer::onMessage,
            this,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3
        )
    );
}

void ChatServer::start(){
    server_.start();
}

void ChatServer::onConnection(
    const::muduo::net::TcpConnectionPtr& conn
){
    if(conn->connected()){
        conn->setContext(HttpContext{});
    }
}

void ChatServer::onMessage(
    const muduo::net::TcpConnectionPtr& conn,
    muduo::net::Buffer* buffer,
    muduo::Timestamp
){
    if(!conn->connected()){
        return;
    }

    auto* context = boost::any_cast<HttpContext>(
        conn->getMutableContext()
    );

    if(!context){
        sendResponse(
            conn,
            HttpResponse(
                500,
                "Internal Server Error",
                R"({"error":"missing contest"})"
            )
        );
        return ;
    }

    const auto result = context->parse(buffer);

    if(result==HttpContext::Result::NeedMore){
        return;
    }

    if(result ==HttpContext::Result::Error){
        sendResponse(
            conn,
            HttpResponse(
                context->errorStatus(),
                context->errorReason(),
                context->errorBody()
            )
        );
        return ;
    }

    const HttpResponse response =
        handleRequest(context->request());
    
    sendResponse(conn,response);
}

HttpResponse ChatServer::handleRequest(
    const HttpRequest& request
){
    if (request.method != "POST" ||
        request.path != "/chat/send") {
        return HttpResponse(
            404,
            "Not Found",
            R"({"error":"route not found"})"
        );
    }

    boost::system::error_code ec;

    const auto data =
        boost::json::parse(request.body, ec);

    if (ec || !data.is_object()) {
        return HttpResponse(
            400,
            "Bad Request",
            R"({"error":"body must be a JSON object"})"
        );
    }

     const auto* question =
        data.as_object().if_contains("question");

    if (!question ||
        !question->is_string() ||
        question->as_string().empty()) {
        return HttpResponse(
            400,
            "Bad Request",
            R"({"error":"question must be a non-empty string"})"
        );
    }

    const std::string text(
        question->as_string().c_str(),
        question->as_string().size()
    );

    boost::json::object result;

    result["success"] = true;
    result["answer"] =
        "[模拟AI]收到你的信息：" + text;

    return HttpResponse(
        200,
        "OK",
        boost::json::serialize(result)
    );
}

void ChatServer::sendResponse(
    const muduo::net::TcpConnectionPtr& conn,
    const HttpResponse& response
) {
    conn->send(response.serialize());

    // 本批仍然是一条连接处理一次请求。
    conn->shutdown();
}