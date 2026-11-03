#include "ChatServer.h"

#include <muduo/net/EventLoop.h>
#include <curl/curl.h>
#include <iostream>

int main(){
    //curl为ai接口做准备
    if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK){
        std::cerr<<"Failed to initialize libcurl\n";
        return 1;
    }
    {
        muduo::net::EventLoop loop;

    ChatServer server(&loop,8080);

    server.start();

    std::cout<<"Listening on http://127.0.0.1:8080\n";

    loop.loop();
    }
    curl_global_cleanup();//清理全局资源
    return 0;
}