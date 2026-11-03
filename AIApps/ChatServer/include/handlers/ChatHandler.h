#pragma once

#include "ai/ChatMessage.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"

#include <vector>

class ChatHandler{
public:
    HttpResponse handle(
        const HttpRequest& request
    );

    HttpResponse history(const HttpRequest& request) const;

    HttpResponse clear(const HttpRequest& request);
private:
    std::vector<ChatMessage> messages_;
};