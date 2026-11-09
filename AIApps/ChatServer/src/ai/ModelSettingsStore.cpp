#include "ai/ModelSettingsStore.h"
#include "storage/DbConnectionPool.h"

#include <cppconn/connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>

#include <cstdlib>
#include <exception>
#include <memory>
#include <stdexcept>

namespace{
std::string userStoreMode() {
    const char* value = std::getenv("AI_USER_STORE");

    return value && *value ? value : "memory";
}

/**
 * 检查当前支持的个人模型模式。
 */
bool validMode(const std::string& mode) {
    return mode == "mock" || mode == "deepseek";
}
}//namespace

ModelSettingsStore::ModelSettingsStore() {
    const auto mode = userStoreMode();

    if (mode != "memory" && mode != "mysql") {
        throw std::invalid_argument("invalid AI_USER_STORE");
    }

    mysqlMode_ = mode == "mysql";
}

bool ModelSettingsStore::find(const std::string& username,std::string& mode)const{
    if(!mysqlMode_){
        const auto it = modes_.find(username);

        if(it==modes_.end()){
            return false;
        }
        mode = it->second;
        return true;
    }

    try{
        auto conn =DbConnectionPool::instance().acquire();

        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement(
                "SELECT mode "
                "FROM user_model_settings "
                "WHERE username = ?"
            )
        );

        stmt->setString(1, username);

        std::unique_ptr<sql::ResultSet> rows(stmt->executeQuery());

        if (!rows->next()) {
            return false;
        }

        const std::string stored =
            std::string(rows->getString("mode"));

        if (!validMode(stored)) {
            throw ModelSettingsError("invalid stored model mode");
        }

        mode = stored;
        return true;
    }catch(const std::exception&){
        throw ModelSettingsError("model settings unavailable");
    }
    
}

void ModelSettingsStore::set(const std::string& username,const std::string& mode){
    
    if (!validMode(mode)) {
        throw std::invalid_argument(
            "invalid model mode"
        );
    }

    if (!mysqlMode_) {
        modes_[username] = mode;
        return;
    }

     try {
        auto conn =DbConnectionPool::instance().acquire();

        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement(
                "INSERT INTO user_model_settings "
                "(username, mode) "
                "VALUES (?, ?) "
                "ON DUPLICATE KEY UPDATE mode = ?"
            )
        );

        stmt->setString(1, username);
        stmt->setString(2, mode);
        stmt->setString(3, mode);

        stmt->executeUpdate();
    } catch (const std::exception&) {
        throw ModelSettingsError(
            "model settings unavailable"
        );
    }
}

void ModelSettingsStore::clear(const std::string& username){
    if (!mysqlMode_) {
        modes_.erase(username);
        return;
    }

    try {
        auto conn =
            DbConnectionPool::instance().acquire();

        std::unique_ptr<sql::PreparedStatement> stmt(
            conn->prepareStatement(
                "DELETE FROM user_model_settings "
                "WHERE username = ?"
            )
        );

        stmt->setString(1, username);
        stmt->executeUpdate();
    } catch (const std::exception&) {
        throw ModelSettingsError(
            "model settings unavailable"
        );
    }
}