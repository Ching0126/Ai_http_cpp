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
/**
 * 从 JSON 对象中读取一个非空字符串字段。
 *
 * @param object JSON 对象。
 * @param name 字段名称。
 * @param result 成功时写入读取到的字符串。
 * @return 字段存在且为非空字符串时返回 true。
 */
bool stringField(
    const boost::json::object& object,
    const char* name,
    std::string& result
){
    const auto* value = object.if_contains(name);

    if(!value||
        !value->is_string()||
        value->as_string().empty()){
        return false;
    }

    const auto& text = value->as_string();

    result.assign(text.c_str(),text.size());

    return true;
}
/**
 * 从请求正文读取 sessionId。
 *
 * 正文不是合法 JSON 对象，或 sessionId 不合法时返回 false。
 */
bool readSessionId(
    const HttpRequest& request,
    std::string& sessionId
){
    boost::system::error_code ec;

    const auto data = boost::json::parse(request.body,ec);

    return !ec&&data.is_object()&&
            stringField(data.as_object(),"sessionId",sessionId);
}

HttpResponse invalidBody(){
    return HttpResponse(
        400,
        "Bad Request",
        R"({"error":"invalid request body"})"
    );
}

HttpResponse missingSession() {
    return HttpResponse(
        404,
        "Not Found",
        R"({"error":"session not found"})"
    );
}
}//namespace
// curl --noproxy '*' -i http://127.0.0.1:8080/chat/send \
//   -H 'Content-Type: application/json' \
//   -d '{"question":"什么是 HTTP？"}'

HttpResponse ChatHandler::createSession(const HttpRequest& request,const std::string& username){//创建会话
    try{
        const auto id = sessions_.create(username);

        boost::json::object result;

        result["success"] = true;
        result["sessionId"]=id;

        return HttpResponse(
            200,
            "OK",
            boost::json::serialize(result)
        );
    }catch(const std::exception&){
        return HttpResponse(
            500,
            "Internal Server Error",
            R"({"error":"cannot create session"})"
        );
    }
}

HttpResponse ChatHandler::listSessions(const HttpRequest& request,const std::string& username){
    boost::json::array ids;

    for(const auto& id : sessions_.list(username)){
        ids.emplace_back(id);
    }

    boost::json::object result;

    result["sessionIds"]=ids;

    return HttpResponse(
        200,
        "OK",
        boost::json::serialize(result)
    );
}

HttpResponse ChatHandler::handle(
    const HttpRequest& request,const std::string& username
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

    std::string sessionId;
    std::string text;

    if (ec || !data.is_object()||
        !stringField(data.as_object(),"sessionId",sessionId)||
        !stringField(data.as_object(),"question",text)) {
        return invalidBody();
    }

    auto* session = sessions_.find(sessionId,username);

    if(!session){
        return missingSession();
    }

    auto pending = session->messages;

    pending.push_back({"user",text});

    const auto mode = envOr("AI_MODE","mock");

    std::string answer;

    if(mode == "mock"){
        answer = "[模拟ai]收到你的信息"+text
                +"此前消息数："+std::to_string(session->messages.size());
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
    result["sessionId"] = sessionId;
    result["answer"] = answer;
    HttpResponse response(
        200,
        "OK",
        boost::json::serialize(result)
    );

    session->messages = std::move(pending);
    return response;
}

HttpResponse ChatHandler::history(const HttpRequest& request,const std::string& username){
    std::string sessionId;

    if(!readSessionId(request,sessionId)){
        return invalidBody();
    }

    const auto* session = sessions_.find(sessionId,username);

    if(!session){
        return missingSession();
    }
    
    boost::json::array entries;

    for(const auto& item:session->messages){
        boost::json::object entry;

        entry["role"] = item.role;
        entry["content"] = item.content;

        entries.push_back(entry);
    }

    boost::json::object result;

    result["sessionId"]=sessionId;
    result["messages"]=entries;

    return HttpResponse(
        200,
        "OK",
        boost::json::serialize(result)
    );
}

HttpResponse ChatHandler::clear(const HttpRequest& request,const std::string& username) {

     std::string sessionId;

    if (!readSessionId(request, sessionId)) {
        return invalidBody();
    }

    auto* session = sessions_.find(sessionId,username);

     if (!session) {
        return missingSession();
    }

    session->messages.clear();

    return HttpResponse(
        200,
        "OK",
        R"({"success":true})"
    );
}
