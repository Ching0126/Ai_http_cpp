#pragma once

#include "http/HttpRequest.h"
#include "http/HttpResponse.h"

#include <string>
//固定静态文件处理器
//文件路径和类型由服务器处理时指定
class StaticFileHandler{
public:
    //静态文件处理器
    StaticFileHandler(
        std::string filePath,
        std::string contentType
    );
    //读取文件，构造http响应
    HttpResponse handle(const HttpRequest& request)const;

private:
    std::string filePath_;
    std::string contentType_;
};