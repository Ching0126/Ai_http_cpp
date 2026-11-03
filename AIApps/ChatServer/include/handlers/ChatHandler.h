#pragma once

#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "session/ChatSessionManager.h"

#include <vector>

class ChatHandler{
public:

    HttpResponse createSession(const HttpRequest& request);

    HttpResponse listSessions(const HttpRequest& request) const;

    HttpResponse handle(const HttpRequest& request);

    HttpResponse history(const HttpRequest& request);

    HttpResponse clear(const HttpRequest& request);
private:
    ChatSessionManager sessions_;
};