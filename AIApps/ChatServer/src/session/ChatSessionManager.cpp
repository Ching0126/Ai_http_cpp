#include "session/ChatSessionManager.h"

#include  <mysql_driver.h>

#include <cppconn/connection.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>

#include <cstdlib>
#include <limits>
#include <memory>
#include <utility>

namespace{

std::string envOr(
    const char* name,
    const char* fallback
) {
    const char* value = std::getenv(name);
    return value && *value ? value : fallback;
}

std::unique_ptr<sql::Connection> connectDb(
    const std::string& host,
    const std::string& user,
    const std::string& password,
    const std::string& database
){
    auto* driver = sql::mysql::get_driver_instance();

    std::unique_ptr<sql::Connection> conn(driver->connect(host,user,password));

    conn->setSchema(database);

    return conn;
}

bool validId(const std::string& id){
    if(id.empty() || id[0]=='0'|| id.size()>20){
        return false;
    }

    std::uint64_t number = 0;

    for(char c : id){
        if (c < '0' || c > '9') {
            return false;
        }

         const unsigned digit =
            static_cast<unsigned>(c - '0');

        const auto max = std::numeric_limits<std::uint64_t>::max();

        if (number > (max - digit) / 10) {
            return false;
        }
        number = number * 10 + digit;
    }
    return true;
}

bool owned (
    sql::Connection& conn,
    const std::string& id,
    const std::string& owner
){
    std::unique_ptr<sql::PreparedStatement> stmt(
        conn.prepareStatement(
            "SELECT id "
            "FROM chat_sessions "
            "WHERE id = ? AND owner = ?"
        )
    );

     stmt->setString(1, id);
    stmt->setString(2, owner);

    std::unique_ptr<sql::ResultSet> rows(
        stmt->executeQuery()
    );

    return rows->next();
}

}//namespace

ChatSessionManager::ChatSessionManager() {
    const auto mode = envOr("AI_CHAT_STORE","memory");

    if (mode != "memory" && mode != "mysql") {
        throw std::invalid_argument(
            "invalid AI_CHAT_STORE"
        );
    }

    mysqlMode_ = mode == "mysql";

    if (
        mysqlMode_ &&envOr("AI_USER_STORE", "memory") != "mysql") {
        throw std::invalid_argument(
            "mysql chat storage requires mysql user storage"
        );
    }

    host_ = envOr("AI_DB_HOST","tcp://127.0.0.1:3306");

    user_ = envOr("AI_DB_USER","ai_http_user");

    password_ = envOr("AI_DB_PASSWORD", "");

    database_ = envOr("AI_DB_NAME","ai_http_learning");
}

std::string ChatSessionManager::create(const std::string& owner) {
    if (!mysqlMode_) {
        if (nextId_ == 0) {
            throw std::overflow_error(
                "session IDs exhausted"
            );
        }

        const auto id = std::to_string(nextId_++);

        sessions_.emplace(
            id,
            ChatSession{id, owner, {}}
        );

        return id;
    }

    try {
        auto conn = connectDb(host_,user_,password_,database_);

        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement(
                "INSERT INTO chat_sessions (owner) "
                "VALUES (?)"
            )
        );

        stmt->setString(1, owner);
        stmt->executeUpdate();

        // LAST_INSERT_ID() 获取当前连接刚生成的自增 ID。
        std::unique_ptr<sql::PreparedStatement> query(
            conn->prepareStatement(
                "SELECT LAST_INSERT_ID() AS id"
            )
        );

        std::unique_ptr<sql::ResultSet> row(
            query->executeQuery()
        );

        if (!row->next()) {
            throw ChatStorageError(
                "missing session ID"
            );
        }

        return std::string(
            row->getString("id")
        );
    } catch (const sql::SQLException&) {
        throw ChatStorageError(
            "chat storage unavailable"
        );
    }
}

ChatSession* ChatSessionManager::find(
    const std::string& id,
    const std::string& owner
) {
    if (!validId(id)) {
        return nullptr;
    }

    if (!mysqlMode_) {
        auto it = sessions_.find(id);

        if (
            it == sessions_.end() ||
            it->second.owner != owner
        ) {
            return nullptr;
        }

        return &it->second;
    }

    try {
        auto conn = connectDb(
            host_,
            user_,
            password_,
            database_
        );

        if (!owned(*conn, id, owner)) {
            return nullptr;
        }

        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement(
                "SELECT role, content "
                "FROM chat_messages "
                "WHERE session_id = ? "
                "ORDER BY id"
            )
        );

        stmt->setString(1, id);

        std::unique_ptr<sql::ResultSet> rows(
            stmt->executeQuery()
        );

        ChatSession session{id, owner, {}};

        while (rows->next()) {
            session.messages.push_back({
                std::string(rows->getString("role")),
                std::string(rows->getString("content"))
            });
        }

        // 找到则替换，找不到则插入。
        auto it = sessions_.insert_or_assign(
            id,
            std::move(session)
        ).first;

        return &it->second;
    } catch (const sql::SQLException&) {
        throw ChatStorageError(
            "chat storage unavailable"
        );
    }
}

std::vector<std::string> ChatSessionManager::list(
    const std::string& owner
) const {
    std::vector<std::string> ids;

    if (!mysqlMode_) {
        for (const auto& entry : sessions_) {
            if (entry.second.owner == owner) {
                ids.push_back(entry.first);
            }
        }

        return ids;
    }

    try {
        auto conn = connectDb(host_,user_,password_,database_);

        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement(
                "SELECT id "
                "FROM chat_sessions "
                "WHERE owner = ? "
                "ORDER BY id"
            )
        );

        stmt->setString(1, owner);

        std::unique_ptr<sql::ResultSet> rows(stmt->executeQuery());

        while (rows->next()) {
            ids.push_back(
                std::string(rows->getString("id"))
            );
        }

        return ids;
    } catch (const sql::SQLException&) {
        throw ChatStorageError("chat storage unavailable");
    }
}

bool ChatSessionManager::save(
    const std::string& id,
    const std::string& owner,
    const std::vector<ChatMessage>& messages
) {
    if (!validId(id)) {
        return false;
    }

    if (!mysqlMode_) {
        auto* session = find(id, owner);

        if (!session) {
            return false;
        }

        session->messages = messages;
        return true;
    }

    try {
        auto conn = connectDb(
            host_,
            user_,
            password_,
            database_
        );

        // 关闭自动提交，开始将以下操作作为一个事务。
        conn->setAutoCommit(false);

        if (!owned(*conn, id, owner)) {
            conn->rollback();
            return false;
        }

        std::unique_ptr<sql::PreparedStatement> erase(
            conn->prepareStatement(
                "DELETE FROM chat_messages "
                "WHERE session_id = ?"
            )
        );

        erase->setString(1, id);
        erase->executeUpdate();

        std::unique_ptr<sql::PreparedStatement> insert(
            conn->prepareStatement(
                "INSERT INTO chat_messages "
                "(session_id, role, content) "
                "VALUES (?, ?, ?)"
            )
        );

        for (const auto& message : messages) {
            insert->setString(1, id);
            insert->setString(2, message.role);
            insert->setString(3, message.content);

            insert->executeUpdate();
        }

        // 删除和插入都成功后，才提交。
        conn->commit();

        return true;
    } catch (const sql::SQLException&) {
        // 连接关闭时，尚未提交的事务会回滚。
        throw ChatStorageError(
            "chat storage unavailable"
        );
    }
}