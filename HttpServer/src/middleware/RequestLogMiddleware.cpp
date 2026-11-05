#include "middleware/RequestLogMiddleware.h"

#include <iostream>

bool RequestLogMiddleware::before(const HttpRequest& request,HttpResponse&){
    std::clog<<"[before]"<<request.method<<" "<<request.path<<std::endl;

    return true;
}

void RequestLogMiddleware::after(const HttpRequest& request,HttpResponse&){
    std::clog<<"[after]"<<request.method<<" "<<request.path<<std::endl;
}