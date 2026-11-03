#include "handlers/ChatHandler.h"
#include "ai/AIClient.h"

#include <boost/json/src.hpp>

#include <cstdlib>
#include <exception>
#include <string>
#include <utility>

namespace {
    //读取环境变量
std::string envOr(const char*name,const char* fallback){
    const char* value = std::getenv(name);

    return value && *value ? value : fallback;
}
}//namespace
// curl --noproxy '*' -i http://127.0.0.1:8080/chat/send \
//   -H 'Content-Type: application/json' \
//   -d '{"question":"什么是 HTTP？"}'

HttpResponse ChatHandler::handle(
    const HttpRequest& request
//request.method  = "POST";
// request.path    = "/chat/send";
// request.version = "HTTP/1.1";

// request.headers = {
//     {"content-type",   " application/json"},
//     {"content-length", " 正文的字节数"}
//     // 还可能有 Host 等其他请求头
// };

// request.body = R"({"question":"你好"})";
){
     boost::system::error_code ec;

    const auto data =boost::json::parse(request.body, ec);//请求解析为json

    if (ec || !data.is_object()) {
        return HttpResponse(
            400,
            "Bad Request",
            R"({"error":"body must be a JSON object"})"
        );
    }

    const auto* question = data.as_object().if_contains("question");

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

    auto pending = messages_;

    pending.push_back({"user",text});

    const auto mode = envOr("AI_MODE","mock");

    std::string answer;

    if(mode == "mock"){
        answer = "[模拟ai]收到你的信息"+text
                +"此前消息数："+std::to_string(messages_.size());
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

        answer = client.ask(pending);
    }catch(const std::exception&){
        return HttpResponse(
            502,
            "Bad Gateway",
            R"({"error":"AI request failed"})"
        );
    }
    }else{//mode什么都不是
        return HttpResponse(
            503,
            "Service Unavailable",
            R"({"error":"invalid AI_MODE"})"
        );
    }

    pending.push_back({"assistant",answer});

    boost::json::object result;

    result["success"] = true;
    result["answer"] = answer;
    HttpResponse response(
        200,
        "OK",
        boost::json::serialize(result)
    );

    messages_ = std::move(pending);
    return response;
}

HttpResponse ChatHandler::history(const HttpRequest& request)const{
    boost::json::array entries;

    for(const auto& item:messages_){
        boost::json::object entry;

        entry["role"] = item.role;
        entry["content"] = item.content;

        entries.push_back(entry);
    }

    boost::json::object result;

    result["messages"]=entries;

    return HttpResponse(
        200,
        "OK",
        boost::json::serialize(result)
    );
}

HttpResponse ChatHandler::clear(
    const HttpRequest&
) {
    messages_.clear();

    return HttpResponse(
        200,
        "OK",
        R"({"success":true})"
    );
}
