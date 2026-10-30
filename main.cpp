#include "ChatServer.h"

#include <muduo/net/EventLoop.h>
#include <iostream>

int main(){
    muduo::net::EventLoop loop;

    ChatServer server(&loop,8080);

    server.start();

    std::cout<<"Listening on http://127.0.0.1:8080\n";

    loop.loop();
}