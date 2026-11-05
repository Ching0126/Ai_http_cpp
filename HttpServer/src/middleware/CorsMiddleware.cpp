#include "middleware/CorsMiddleware.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <utility>

namespace{

std::string header(const HttpRequest& request,const char* name){
    const auto it = request.headers.find(name);

    if(it==request.headers.end()){
        return {};
    }

    const auto start = it->second.find_first_not_of(" \t");

    if(start==std::string::npos){
        return {};
    }

    const auto end = it->second.find_last_not_of(" \t");

    return it->second.substr(start,end-start+1);

}

bool allowedHeaders(std::string value){
    if(value.empty()){
        return true;
    }

    std::size_t start = 0;

    while(start<value.size()){
        const auto end = value.find(',',start);

        std::string part = value.substr(start,end==std::string::npos?end:end-start);

        const auto first = part.find_first_not_of(" \t");

        const auto last = part.find_last_not_of(" \t");

        if(first==std::string::npos){
            return false;
        }

        part = part.substr(first,last-first+1);

        std::transform(part.begin(),part.end(),part.begin(),[](unsigned char c){
            return static_cast<char>(std::tolower(c));
        });

        if(part != "content-type"){
            return false;
        }

        if(end == std::string::npos){
            return true;
        }

        start = end+1;

        if(start == value.size()){
            return false;
        }
    }
    return false;
}

}//namespace

CorsMiddleware::CorsMiddleware(std::string allowOrigin)
    :allowOrigin_(allowOrigin){
    if(allowOrigin_ == "*" ||allowOrigin_.find_first_of("\r\n")!= std::string::npos){
        throw std::invalid_argument("invalid CORS origin");
    }
}

bool CorsMiddleware::before(const HttpRequest& request,HttpResponse& response){
    const auto origin = header(request,"origin");

    const auto method = header(request,"access-control-request-method");

    if(request.method != "OPTIONS"||
        origin.empty()||
        method.empty()){
        return true;
    }

    if(allowOrigin_.empty()||origin != allowOrigin_ ||
        (method != "GET" &&method != "POST")||
        !allowedHeaders(header(request,"access-control-request-headers"))){
        response = HttpResponse(
            403,
            "Forbidden",
            R"({"error":"CORS preflight denied"})"
        );

        return false;
    }

    response = HttpResponse(
        204,
        "No Content",
        ""
    );

    response.addHeader(
        "Access-Control-Allow-Methods",
        "GET, POST"
    );

    response.addHeader(
        "Access-Control-Allow-Headers",
        "Content-Type"
    );

    // 预检已经处理，不再进入路由。
    return false;
}

void CorsMiddleware::after(const HttpRequest& request,HttpResponse& response){
    if(allowOrigin_.empty()){
        return ;
    }

    response.addHeader("Vary","Origin");

    const auto origin = header(request,"origin");

    if(origin==allowOrigin_){
        response.addHeader("Access-Control-Allow-Origin",allowOrigin_);

        response.addHeader("Access-Control-Allow-Credentials","true");
    }

}