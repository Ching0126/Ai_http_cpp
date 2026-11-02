#pragma once

#include "HttpRequest.h"

#include <muduo/net/Buffer.h>
#include <cstddef>
#include <string>
//保存一个连接当前的解析状态，解析请求行、请求头、请求正文
// - NeedMore：数据还不够，等待下一次回调。
// - Complete：请求完整，可以交给业务处理。
// - Error：请求不符合当前支持的格式。
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
        Body,//
        Complete,//
        Error//
    };

    Result fail(
        int status,
        const std::string& reason,
        const std::string& body
    );

    bool parseRequestLine(const std::string& line);//解析一行请求行

    bool parseHeader(const std::string& line);//解析一行请求头

    State state_ = State::RequestLine;

    HttpRequest request_;

    std::size_t headerBytes_ =0;//已消费的请求头和请求行的总字节数

    std::size_t contentLength_ = 0;//正文应该有多少字节。
    bool hasContentLength_ = false;//有没有找到过 Content-Length。
    //报错日志
    int errorStatus_=0;
    std::string errorReason_;
    std::string errorBody_;
};