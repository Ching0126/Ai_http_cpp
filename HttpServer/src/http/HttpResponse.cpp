#include "http/HttpResponse.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

void HttpResponse::addHeader(
    const std::string& name,
    const std::string& value
){
    const bool validName = !name.empty() && 
        std::all_of(name.begin(),name.end(),[](unsigned char c){
             return
                    (c >= 'a' && c <= 'z') ||
                    (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') ||
                    c == '-';
        });

    if(!validName || value.find_first_of("\r\n")!=std::string::npos){
        throw std::invalid_argument("invalid header");
    }

    std::string lower = name;

    std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){
        return static_cast<char>(std::tolower(c));
    });

     // 避免和现有响应生成逻辑产生重复头。
    if (lower == "content-length" ||
        lower == "content-type" ||
        lower == "connection" ||
        lower == "set-cookie") {
        throw std::invalid_argument(
            "reserved header"
        );
    }

    extraHeaders_[lower] = value;
}

HttpResponse::HttpResponse(
    int status,
    std::string reason,
    std::string body
)
    :status_(status)
    ,reason_(std::move(reason))
    ,body_(std::move(body)){
}

void HttpResponse::setCookie(const std::string& cookie){//若存在换行符，说明可能吧其他行的信息带进来了
    if(cookie.find_first_of("\r\n")!=std::string::npos){
        throw std::invalid_argument("invalid cookie");
    }
    cookie_ = cookie;
}


    // HTTP/1.1 200 OK                              ← 状态行
    // Content-Type: application/json;charset=utf-8 ← 响应头 默认填写
    // Content-Length: 19                           ← 响应头 默认填写
    // Connection: close                            ← 响应头 默认填写
    //                                              ← 空行   默认填写
    // {"message":"hello"}                          ← 响应正文
std::string HttpResponse::serialize()const{//把响应转化为http报文
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
    "Content-Type: "+contentType_+"\r\n";

    response +=
        "Content-Length: "+
        std::to_string(body_.size())+
        "\r\n";
    
    response += "Connection: close\r\n";

    if(!cookie_.empty()){
        response += "Set-Cookie: "+cookie_ +"\r\n";
    }

    for (const auto& entry : extraHeaders_) {
    response +=
        entry.first +
        ": " +
        entry.second +
        "\r\n";
}

    response +="\r\n";

    response += body_;

    return response;
}

void HttpResponse::setContentType(const std::string& contentType){
    if(contentType.empty()||contentType.find_first_of("\r\n")!=std::string::npos){
        throw std::invalid_argument("invalid content type");
    }

    contentType_ = contentType;
}
