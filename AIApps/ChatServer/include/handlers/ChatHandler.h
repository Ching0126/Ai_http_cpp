#pragma once

#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "session/ChatSessionManager.h"
#include "ai/ModelSettingsStore.h"
#include "tools/ToolRegistry.h"

class ChatHandler{
public:

    HttpResponse createSession(const HttpRequest& request,const std::string& username);

    HttpResponse listSessions(const HttpRequest& request,const std::string& username);

    HttpResponse handle(const HttpRequest& request,const std::string& username);

    HttpResponse history(const HttpRequest& request,const std::string& username);

    HttpResponse clear(const HttpRequest& request,const std::string& username);

    HttpResponse getModelSettings(const HttpRequest& request,const std::string& username);

    HttpResponse setModelSettings(const HttpRequest& request,const std::string& username);
    
private:
    ChatSessionManager sessions_;

    ModelSettingsStore modelSettings_;

    ToolRegistry tools_;
};