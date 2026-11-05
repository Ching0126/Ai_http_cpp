#pragma once 

#include "http/HttpRequest.h"
#include "http/HttpResponse.h"

class Middleware{
public:
    virtual ~Middleware() = default;

    /**
     * 路由处理前执行。
     *
     * @param request 当前 HTTP 请求。
     * @param response 可以在提前结束请求时写入响应。
     * @return true 继续执行；false 直接使用 response。
     */

    virtual bool before(
        const HttpRequest& request,
        HttpResponse& response
    ) = 0;
     /**
     * 路由处理结束后执行。
     *
     * 也会对已经执行 before()、随后提前结束的中间件调用。
     */
    virtual void after(
        const HttpRequest& request,
        HttpResponse& response
    )=0;
};