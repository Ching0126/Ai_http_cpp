#pragma once 

#include "middleware/Middleware.h"
//请求日志中间件
//记录请求方法和路径，方便观察执行顺序
class RequestLogMiddleware : public Middleware{
public:
    bool before(const HttpRequest& request,HttpResponse& response)override;

    void after(const HttpRequest& request,HttpResponse& response)override;
};