#include "router/Router.h"

#include <stdexcept>
#include <utility>

void Router::addRoute(
    const std::string& method,
    const std::string& path,
    Handler handler
) {
    if(!handler){
        throw std::invalid_argument("empty  route handler");
    }
    
    const auto result = routes_.emplace(
      std::make_pair(method,path),
      std::move(handler)  
    );

    if(!result.second){// // emplace 的 second 表示是否成功插入
        throw std::invalid_argument(
            "duplicate route"
        );
    }
}

HttpResponse Router::route(const HttpRequest& request)const{
    const auto it = routes_.find({request.method,request.path});

    if(it == routes_.end()){
        return HttpResponse(
            404,
            "Not Found",
            R"({"error":"route not found"})"
        );
    }
    return it->second(request);
}