#pragma once 

#include "middleware/Middleware.h"

#include <string>

class CorsMiddleware:public Middleware{
public:
    explicit CorsMiddleware(std::string allowOrigin);

    bool before(const HttpRequest& request,HttpResponse& response)override;

    void after(const HttpRequest& request,HttpResponse& response)override;
private:
    std::string allowOrigin_;
};