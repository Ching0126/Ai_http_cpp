#include "messaging/ChatEventStore.h"
#include "storage/DbConnectionPool.h"

#include <boost/json/src.hpp>

#include <cppconn/datatype.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
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

bool ChatEventStore::save(const std::string& body)const{
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

    const bool hasEventId = object.if_contains("eventId") !=nullptr;

    std::string eventId;

    if(hasEventId){
        eventId = readString(object,"eventId");

        const bool valid = std::all_of(eventId.begin(),eventId.end(),[](unsigned char c){
        return (c>='0'&&c<='9'||c>='a'&&c<='f');
        });

        if(eventId.size()!=32 ||!valid){
            throw InvalidChatEvent("invalid event id");
        }
    }
    //格式已经检查正确，可以写入数据库
    auto conn = DbConnectionPool::instance().acquire();

    try{
        std::unique_ptr<sql::PreparedStatement> stmt(
        conn->prepareStatement(
            "INSERT INTO chat_event_logs "
            "(event_type, username, session_id,event_id) "
            "VALUES (?, ?, ?,?)"
        ));

            stmt->setString(1, type);
            stmt->setString(2, username);
            stmt->setString(3, sessionId);

             if (hasEventId) {
            stmt->setString(4, eventId);
            } else {
                stmt->setNull(4,sql::DataType::CHAR);
                }

        stmt->executeUpdate();

        return true;
    }catch(const sql::SQLException& error){
        // 1062 是 MySQL 的唯一键冲突。
        if (
            !hasEventId ||
            error.getErrorCode() != 1062
        ) {
            throw;
        }

        // 查询已有事件，确认冲突确实来自相同事件。
        std::unique_ptr<sql::PreparedStatement> query(
            conn->prepareStatement(
                "SELECT event_type, username, session_id "
                "FROM chat_event_logs "
                "WHERE event_id = ?"
            )
        );

        query->setString(1, eventId);

        std::unique_ptr<sql::ResultSet> rows(
            query->executeQuery()
        );

        if (!rows->next()) {
            // 没有找到这个 eventId，保留原数据库异常。
            throw;
        }

        if (
            std::string(rows->getString("event_type")) != type ||
            std::string(rows->getString("username")) != username ||
            std::string(rows->getString("session_id")) != sessionId
        ) {
            // 同一 ID 对应不同内容，不当作正常重复消息。
            throw InvalidChatEvent(
                "event id conflict"
            );
        }

        // 已存在内容相同的事件，不再插入。
        return false;
    }
    

}