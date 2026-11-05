#include "handlers/StaticFileHandler.h"

#include <fstream>
#include <iterator>
#include <utility>

StaticFileHandler::StaticFileHandler(
    std::string filePath,
    std::string contentType
)   :filePath_(filePath)
    ,contentType_(contentType){

}

HttpResponse StaticFileHandler::handle(const HttpRequest& request)const{
    std::ifstream file(filePath_,std::ios::binary);

    if(!file){
        return HttpResponse(
            500,
            "Internal Server Error",
            R"({"error":"static file unavailable"})"
        );
    }

    const std::string body(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    if(file.bad()){
        return HttpResponse(
            500,
            "Internal Server Error",
            R"({"error":"static file  read failed"})"
        );
    }

    HttpResponse response(
        200,
        "OK",
        body
    );

    response.setContentType(contentType_);

    return response;
}