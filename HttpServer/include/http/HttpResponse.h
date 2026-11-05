#pragma once 

#include <string>
#include <map>
//负责描述响应，并把它转化为http报文
class HttpResponse{
public:
    HttpResponse(
        int status,
        std::string reason,
        std::string body
    );

    //添加响应头
    void addHeader(
        const std::string& name,
        const std::string& value
    );

    void setCookie(const std::string& cookie);

    std::string serialize()const;

    void setContentType(const std::string& contentType);

private:
    int status_;
    std::string reason_;
    std::string body_;

    std::string cookie_;

    std::string contentType_ = "application/json;charset=utf-8";

    std::map<std::string, std::string> extraHeaders_;

    // HTTP/1.1 200 OK                              ← 状态行
    // Content-Type: application/json;charset=utf-8 ← 响应头 默认填写
    // Content-Length: 19                           ← 响应头 默认填写
    // Connection: close                            ← 响应头 默认填写
    //                                              ← 空行   默认填写
    // {"message":"hello"}                          ← 响应正文
};