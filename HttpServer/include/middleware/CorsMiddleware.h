#pragma once 

#include "middleware/Middleware.h"

#include <string>
//CORS中间件
class CorsMiddleware:public Middleware{
//空字符串表示不启用跨域
public:
    explicit CorsMiddleware(std::string allowOrigin);

    bool before(const HttpRequest& request,HttpResponse& response)override;

    //对允许的来源添加CORS响应头
    void after(const HttpRequest& request,HttpResponse& response)override;
private:
    std::string allowOrigin_;
};