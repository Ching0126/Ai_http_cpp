#pragma once

#include "HttpRequest.h"
#include "HttpResponse.h"

class ChatHandler{
public:
    static HttpResponse handle(
        const HttpRequest& request
    );
};