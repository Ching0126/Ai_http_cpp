#pragma once

#include "HttpRequest.h"

#include <muduo/net/Buffer.h>
#include <cstddef>
#include <string>

class HttpContext{
public:
    enum class Result{
        NeedMore,
        Complete,
        Error
    };

    Result parse(muduo::net::Buffer* buffer);

    const HttpRequest& request()const{
        return request_;
    }

    int errorStatus()const{
        return errorStatus_;
    }

    const std::string& errorReason() const{
        return errorReason_;
    }

    const std::string& errorBody()const{
        return errorBody_;
    }
private:
    enum class State{
        RequestLine,
        Headers,
        Body,
        Complete,
        Error
    };

    Result fail(
        int status,
        const std::string& reason,
        const std::string& body
    );

    bool parseRequestLine(const std::string& line);//解析请求行

    bool parseHeader(const std::string& line);//解析一行请求头

    State state_ = State::RequestLine;

    HttpRequest request_;

    std::size_t headerBytes_ =0;//已消费的请求头和请求行的总字节数

    std::size_t contentLength_ = 0;
    bool hasContentLength_ = false;

    int errorStatus_=0;
    std::string errorReason_;
    std::string errorBody_;
};