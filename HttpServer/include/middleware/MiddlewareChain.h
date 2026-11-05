#pragma once 

#include "middleware/Middleware.h"

#include <functional>
#include <memory>
#include <vector>

class MiddlewareChain{
public:
    void add(std::shared_ptr<Middleware> middleware);

    HttpResponse process(
        const HttpRequest& request,
        const std::function<HttpResponse(const HttpRequest&)>& route
    )const;
    
private:
    std::vector<std::shared_ptr<Middleware>> middlewares_;
};