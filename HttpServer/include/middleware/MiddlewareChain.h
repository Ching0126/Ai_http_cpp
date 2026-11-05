#pragma once 

#include "middleware/Middleware.h"

#include <functional>
#include <memory>
#include <vector>
//保存并执行多个中间件
class MiddlewareChain{
public:
    //添加中间件，在服务启动前完成注册
    void add(std::shared_ptr<Middleware> middleware);
    
    //执行完整中间件和路由流程
    //before按注册顺序执行，after按照相反顺序执行
    HttpResponse process(
        const HttpRequest& request,
        const std::function<HttpResponse(const HttpRequest&)>& route
    )const;

private:
    std::vector<std::shared_ptr<Middleware>> middlewares_;
};