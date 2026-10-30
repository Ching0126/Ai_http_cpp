#include "HttpResponse.h"

#include <utility>

HttpResponse::HttpResponse(
    int status,
    std::string reason,
    std::string body
)
    :status_(status)
    ,reason_(std::move(reason))
    ,body_(std::move(body)){
}

std::string HttpResponse::serialize()const{
    std::string response;
    //状态行
    response +=
        "HTTP/1.1 "+
        std::to_string(status_)+
        " "+
        reason_+
        "\r\n";

    //响应头
    response +=
    "Content-Type: application/json;charset=utf-8\r\n";

    response +=
        "Content-Length: "+
        std::to_string(body_.size())+
        "\r\n";
    
    response += "Connection: close\r\n";

    response +="\r\n";

    response += body_;

    return response;
}