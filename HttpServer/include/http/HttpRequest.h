#pragma once

#include<string>
#include<unordered_map>

struct HttpRequest{
    std::string method;
    std::string path;
    std::string version;

    std::unordered_map<std::string,std::string> headers;

    std::string body;

    // POST /chat/send HTTP/1.1
    // Content-Length: 16

    // {"question":"x"}
};