#pragma once

#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "session/ChatSessionManager.h"

class ChatHandler{
public:

    HttpResponse createSession(const HttpRequest& request,const std::string& username);

    HttpResponse listSessions(const HttpRequest& request,const std::string& username);

    HttpResponse handle(const HttpRequest& request,const std::string& username);

    HttpResponse history(const HttpRequest& request,const std::string& username);

    HttpResponse clear(const HttpRequest& request,const std::string& username);
private:
    ChatSessionManager sessions_;
};