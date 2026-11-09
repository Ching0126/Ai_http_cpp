#include "handlers/ChatHandler.h"
#include "ai//ChatStrategyFactory.h"
#include "messaging/ChatEventPublisher.h"

#include <boost/json/src.hpp>
#include <openssl/rand.h>

#include <cstdlib>
#include <exception>
#include <string>
#include <utility>
#include <iostream>
#include <stdexcept>

namespace {
    //读取环境变量
std::string envOr(const char*name,const char* fallback){
    const char* value = std::getenv(name);

    return value && *value ? value : fallback;
}

std::string makeEventId(){//eventid随机生成函数
    unsigned char bytes[16];

    if(RAND_bytes(bytes,sizeof bytes)!=1){
        throw std::runtime_error("cannot generate event id");
    }

    constexpr char hex[]="0123456789abcdef";

    std::string id;
    id.reserve(32);


    for (unsigned char byte : bytes) {
        id += hex[byte >> 4];
        id += hex[byte & 15];
    }

    return id;
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


/**
 * 查询当前用户生效的模型模式。
 *
 * 没有个人设置时，使用环境变量 AI_MODE；
 * 环境变量未设置时默认使用 mock。
 */
HttpResponse ChatHandler::getModelSettings(
    const HttpRequest&,
    const std::string& username
) {
    std::string mode;

    const bool personal = modelSettings_.find(username, mode);

    if (!personal) {
        mode = envOr("AI_MODE", "mock");
    }

    boost::json::object result;

    result["mode"] = mode;
    result["source"] = personal? "user": "default";

    return HttpResponse(
        200,
        "OK",
        boost::json::serialize(result)
    );
}

/**
 * 设置当前用户的模型模式。
 *
 * 正文支持：
 * {"mode":"mock"}
 * {"mode":"deepseek"}
 * {"mode":"default"}  // 删除个人设置，恢复服务默认
 */
HttpResponse ChatHandler::setModelSettings(const HttpRequest& request,const std::string& username) {
    boost::system::error_code ec;

    const auto data = boost::json::parse(request.body,ec);

    std::string mode;

    if (ec ||!data.is_object() ||!stringField(data.as_object(), "mode", mode) ||
        (mode != "default" &&mode != "mock" &&mode != "deepseek")) {
        return invalidBody();
    }

    if (mode == "default") {
        modelSettings_.clear(username);
    } else {
        modelSettings_.set(username, mode);
    }

    // 返回更新后生效的设置。
    return getModelSettings(request, username);
}

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
    }catch (const ChatStorageError&) {
        // 交给 ChatServer 统一返回 503。
        throw;
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

    std::string answer;
    
    if(text=="/time"){
        try{
            answer = "当前服务时间： "+ tools_.invoke("get_time",{});
        }catch (const std::exception&) {
            return HttpResponse(
                503,
                "Service Unavailable",
                R"({"error":"tool unavailable"})"
            );
        }
    } else {
        // 其他输入继续使用用户选择的模型策略。
        try {
            std::string mode;

            if (!modelSettings_.find(username, mode)) {
                mode = envOr("AI_MODE", "mock");
            }

            const auto strategy =
                ChatStrategyFactory::create(mode);

            answer = strategy->ask(pending);
        } catch (const AIConfigError& error) {
            boost::json::object body;

            body["error"] = error.what();

            return HttpResponse(
                503,
                "Service Unavailable",
                boost::json::serialize(body)
            );
        } catch (const AIRequestError&) {
            return HttpResponse(
                502,
                "Bad Gateway",
                R"({"error":"AI request failed"})"
            );
        }
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

     if (!sessions_.save(sessionId, username, pending)) {
        return missingSession();
    }

    // 聊天保存成功后，按配置尝试发布事件。
    if (ChatEventPublisher::enabled()) {//是否启动发布事件？
        try {
            boost::json::object event;

            event["eventId"] = makeEventId();
            event["type"] = "chat.completed";
            event["username"] = username;
            event["sessionId"] = sessionId;

            ChatEventPublisher::publish(boost::json::serialize(event));
        } catch (const std::exception&) {
            // 本批采用尽力发布：
            // 聊天记录已保存，事件失败不改变聊天响应。
            std::clog<< "[chat-event] publish failed"<< std::endl;
        }
    }

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

    if (!sessions_.save(sessionId, username, {})) {
        return missingSession();
    }

    return HttpResponse(
        200,
        "OK",
        R"({"success":true})"
    );
}
