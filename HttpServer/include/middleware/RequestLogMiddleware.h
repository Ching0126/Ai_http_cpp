#pragma once 

#include "middleware/Middleware.h"

class RequestLogMiddleware : public Middleware{
public:
    bool before(const HttpRequest& request,HttpResponse& response)override;

    void after(const HttpRequest& request,HttpResponse& response)override;
};