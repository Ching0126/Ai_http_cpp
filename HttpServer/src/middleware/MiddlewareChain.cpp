#include "middleware/MiddlewareChain.h"

#include <stdexcept>
#include <utility>

void MiddlewareChain::add(std::shared_ptr<Middleware> middleware){
    if(!middleware){
        throw std::invalid_argument("middleware is null");
    }

    middlewares_.push_back(std::move(middleware));
}

HttpResponse MiddlewareChain::process(
    const HttpRequest& request,
    const std::function<HttpResponse(const HttpRequest&)>& route//这个类型可以包装符合上述要求的可调用对象，只要接收返回一致，就可以调用它
)const{
    HttpResponse response(
        500,
        "Internal Server Error",
        R"({"error":"request on handled"})"
    );

    std::size_t excuted = 0;//记录有多少个中间件执行过before

    bool proceed = true;//记录是否执行路由？
    //按照顺序执行before
    for(const auto& middleware:middlewares_){
        ++excuted;

        if(!middleware->before(request,response)){
            proceed = false;
            break;
        }
    }

    if(proceed){//所有的before都同意，就执行路由
        response = route(request);
    }

    while(excuted >0){
        --excuted;

        middlewares_[excuted]->after(request,response);
    }
    return response;
}