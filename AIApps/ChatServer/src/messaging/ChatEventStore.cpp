#include "messaging/ChatEventStore.h"
#include "storage/DbConnectionPool.h"

#include <boost/json/src.hpp>

#include <cppconn/connection.h>
#include <cppconn/prepared_statement.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>

namespace{
//json转字符串
std::string readString(const boost::json::object& object,const char* key){
    const auto* value = object.if_contains(key);

    if(!value || !value->is_string()){
        throw InvalidChatEvent("invalid field");
    }
    const auto& text = value->as_string();

    return std::string(text.data(),text.size());
}
//判断用户名是否合法？
bool validUsername(const std::string& username){
    if(username.size()<3 ||username.size()>32){
        return false;
    }

    return std::all_of(username.begin(),username.end(),[](unsigned char c){
        return (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') ||
                c == '_';
    });
}
//检查id是否合法？
bool validSessionId(const std::string& id){
    if(id.empty()||id.size()>20||id[0]=='0'){
        return false;
    }

    std::uint64_t value = 0;

    for(char c:id){
        if(c<'0'||c>'9'){
            return false;
        }

        const unsigned digit = static_cast<unsigned>(c-'0');

        const auto max = std::numeric_limits<std::uint64_t>::max();//获取整数类型的最大值

        if(value>(max-digit)/10){//检查下一步不会超过上限
            return false;
        }
        value = value*10 + digit;
    }
    return true;
}
}//namespace

void ChatEventStore::save(const std::string& body)const{
    if(body.size()>4096){//限制正文的大小
        throw InvalidChatEvent("event too large");
    }

    boost::system::error_code ec;

    const auto parsed = boost::json::parse(body,ec);

    if(ec||!parsed.is_object()){
        throw InvalidChatEvent("invalid JSON event");
    }

    const auto& object = parsed.as_object();

    const auto type = readString(object,"type");

    const auto username = readString(object,"username");

    const auto sessionId = readString(object,"sessionId");

    if(type != "chat.completed"||
        !validUsername(username)||
        !validSessionId(sessionId)){
        throw InvalidChatEvent("invalid chat event");
    }
    //格式已经检查正确，可以写入数据库
    auto conn = DbConnectionPool::instance().acquire();

    std::unique_ptr<sql::PreparedStatement> stmt(
        conn->prepareStatement(
            "INSERT INTO chat_event_logs "
            "(event_type, username, session_id) "
            "VALUES (?, ?, ?)"
        )
    );

    stmt->setString(1, type);
    stmt->setString(2, username);
    stmt->setString(3, sessionId);

    stmt->executeUpdate();

}