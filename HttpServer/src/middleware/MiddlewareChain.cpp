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
    const std::function<HttpResponse(const HttpRequest&)>& route
)const{
    HttpResponse response(
        500,
        "Internal Server Error",
        R"({"error":"request on handled"})"
    );

    std::size_t excuted = 0;

    bool proceed = true;

    for(const auto& middleware:middlewares_){
        ++excuted;

        if(!middleware->before(request,response)){
            proceed = false;
            break;
        }
    }

    if(proceed){
        response = route(request);
    }

    while(excuted >0){
        --excuted;

        middlewares_[excuted]->after(request,response);
    }
    return response;
}