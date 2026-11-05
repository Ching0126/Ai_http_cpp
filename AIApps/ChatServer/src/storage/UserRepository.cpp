#include "storage/UserRepository.h"

#include <mysql_driver.h>

#include <cppconn/connection.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>

#include <cstdlib>
#include <memory>
#include <stdexcept>

namespace{

std::string envOr(
    const char* name,
    const char* fallback
){
    const char* value = std::getenv(name);

    return value && *value ? value : fallback;
}

template<std::size_t N>
std::string hexEnCode(const std::array<unsigned char,N>& bytes){
    constexpr char digits[]="0123456789abcdef";

    std::string output;
    output.reserve(N*2);

    for(unsigned char b : bytes){
        output += digits[b>>4];
        output += digits[b&15];
    }
    return output;
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }

    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }

    return -1;
}

template<std::size_t N>
void hexDecode(
    const std::string& text,
    std::array<unsigned char,N>& result
){
    if(text.size()!=N*2){
        throw std::runtime_error("invalid stored hash");
    }

    for(std::size_t i =0;i<N;++i){
        const int high = hexValue(text[2*i]);

        const int low = hexValue(text[2*i+1]);

        if(high<0||low<0){
            throw std::runtime_error("invalid stored hash");
        }

        result[i]=static_cast<unsigned char>((high<<4)|low);
    }
}

std::unique_ptr<sql::Connection> connectDb(
    const std::string& host,
    const std::string& user,
    const std::string& password,
    const std::string& database
){
    auto* driver = sql::mysql::get_mysql_driver_instance();

    std::unique_ptr<sql::Connection> conn(driver->connect(host,user,password));

    conn->setSchema(database);

    return conn;
}
}

UserRepository::UserRepository(){
    const auto mode = envOr("AI_USER_STORE","memory");

    if(mode != "memory"&&mode != "mysql"){
        throw std::invalid_argument("invalid AI_USER_STORE");
    }

    mysqlMode_ = mode =="mysql";

    host_ = envOr("AI_DB_HOST","tcp://127.0.0.1:3306");

    user_ = envOr("AI_DB_USER","ai_http_user");

    password_ = envOr("AI_DB_PASSWORD","");

    database_ = envOr("AI_DB_NAME","ai_http_learning");
}

bool UserRepository::create(
    const std::string& username,
    const UserRecord& user
){
    if(!mysqlMode_){
        return users_.emplace(username,user).second;
    }

    auto conn = connectDb(host_,user_,password_,database_);

    try{
        std::unique_ptr<sql::PreparedStatement>
        stmt(
            conn->prepareStatement(
                "INSERT INTO users "
                "(username, salt_hex, hash_hex) "
                "VALUES (?, ?, ?)"
            )
        );

        stmt->setString(1,username);
        stmt->setString(2,hexEnCode(user.salt));
        stmt->setString(3,hexEnCode(user.hash));

        stmt->executeUpdate();
        return true;
    }catch (const sql::SQLException& error){
        if(error.getErrorCode()==1062){
            return false;
        }

        throw ;
    }
}

bool UserRepository::find(
    const std::string& username,
    UserRecord& user
)const{
    if(!mysqlMode_){
        const auto it = users_.find(username);

        if(it==users_.end()){
            return false;
        }

        user=it->second;

        return true;
    }

    auto conn = connectDb(host_,user_,password_,database_);

    std::unique_ptr<sql::PreparedStatement>
    stmt(
        conn->prepareStatement(
            "SELECT salt_hex, hash_hex "
            "FROM users WHERE username = ?"
        ));

    stmt->setString(1, username);

    std::unique_ptr<sql::ResultSet> result(
        stmt->executeQuery()
    );

    if (!result->next()) {
        return false;
    }

    hexDecode(
        std::string(result->getString("salt_hex")),
        user.salt
    );

    hexDecode(
        std::string(result->getString("hash_hex")),
        user.hash
    );

    return true;

}