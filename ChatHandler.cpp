#include "ChatHandler.h"
#include "AIClient.h"
#include <boost/json/src.hpp>

#include <cstdlib>
#include <exception>
#include <string>


namespace {
std::string envOr(const char*name,const char* fallback){
    const char* value = std::getenv(name);

    return value && *value ? value : fallback;
}
}//namespace

HttpResponse ChatHandler::handle(
    const HttpRequest& request
){
     boost::system::error_code ec;

    const auto data =
        boost::json::parse(request.body, ec);

    if (ec || !data.is_object()) {
        return HttpResponse(
            400,
            "Bad Request",
            R"({"error":"body must be a JSON object"})"
        );
    }

    const auto* question =
        data.as_object().if_contains("question");

    if (!question ||
        !question->is_string() ||
        question->as_string().empty()) {
        return HttpResponse(
            400,
            "Bad Request",
            R"({"error":"question must be a non-empty string"})"
        );
    }

    const std::string text(
        question->as_string().c_str(),
        question->as_string().size()
    );

    const auto mode = envOr("AI_MODE","mock");

    std::string answer;

    if(mode == "mock"){
        answer = "[模拟ai]收到你的信息"+text;
    }else if (mode == "deepseek"){
        const auto key = envOr("DEEPSEEK_API_KEY","");

    if(key.empty()){
        return HttpResponse(
            503,
            "Service Unavailable",
            R"({"error":"API key is not configured"})"
        );
    }

    const auto url = envOr("DEEPSEEK_API_URL","https://api.deepseek.com/chat/completions");

    const auto model = envOr("DEEPSEEK_MODEL","deepseek-flash");

    try{
        AIClient client(key,url,model);

        answer = client.ask(text);
    }catch(const std::exception&){
        return HttpResponse(
            502,
            "Bad Gateway",
            R"({"error":"AI request failed"})"
        );
    }
    }else{
        return HttpResponse(
            503,
            "Service Unavailable",
            R"({"error":"invalid AI_MODE"})"
        );
    }
    boost::json::object result;

    result["success"] = true;
    result["answer"] = answer;
    return HttpResponse(
        200,
        "OK",
        boost::json::serialize(result)
    );
}