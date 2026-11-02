#pragma once

#include "HttpRequest.h"
#include "HttpResponse.h"

#include <functional>
#include <map>
#include <string>
#include <utility>

class Router{
public:
    using Handler = std::function<HttpResponse(const HttpRequest&)>;

    //注册路由
    void addRoute(
        const std::string& method,
        const std::string& path,
        Handler handler
    );
    //根据请求查找并执行处理函数
    HttpResponse route(
        const HttpRequest& request
    )const;

private:

    std::map<
        std::pair<std::string,std::string>,
        Handler
    > routes_;
};