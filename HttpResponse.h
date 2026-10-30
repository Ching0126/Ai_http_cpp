#pragma once 

#include<string>

class HttpResponse{
public:
    HttpResponse(
        int status,
        std::string reason,
        std::string body
    );

    std::string serialize()const;
private:
    int status_;
    std::string reason_;
    std::string body_;
};