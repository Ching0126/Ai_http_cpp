#include "auth/AuthHandler.h"

#include <boost/json.hpp>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <algorithm>
#include <string_view>

namespace{
//浏览器收到后，会保存这个 Cookie。
constexpr int iterations = 120000;
//读取用户名和密码，并检查格式
bool credentials(
    const HttpRequest& request,
    std::string& username,
    std::string& password
){
    boost::system::error_code ec;

    const auto data = boost::json::parse(request.body,ec);

    if(ec||!data.is_object()){
        return false;
    }

    const auto* u = data.as_object().if_contains("username");

    const auto* p = data.as_object().if_contains("password");

    if(!u ||!p ||!u->is_string()||!p->is_string()){
            return false;
    }

    username.assign(u->as_string().c_str(),u->as_string().size());
    password.assign(p->as_string().c_str(),p->as_string().size());

    if(username.size()<3 || username.size()>32||password.size()<8||password.size()>128){
        return false;
    }

    return std::all_of(
        username.begin(),
        username.end(),
        [](unsigned char c){
            return 
                (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') ||
                c == '_';
        }
    );
}
/**
 * 使用 PBKDF2-HMAC-SHA256 计算密码摘要。
 *
 * @param password 用户输入的密码。
 * @param salt 注册时生成的随机盐。
 * @param out 成功时写入 32 字节摘要。
 */
//根据用户输入的密码和随机盐，计算出一段固定长度的数据，用于以后验证密码。
bool derive(
    const std::string& password,
    const std::array<unsigned char,16>& salt,//随即盐
    std::array<unsigned char,32>& out
){
    return  PKCS5_PBKDF2_HMAC(
        password.data(),
        static_cast<int>(password.size()),
        salt.data(),
        static_cast<int>(salt.size()),
        iterations,
        EVP_sha256(),
        static_cast<int>(out.size()),
        out.data()
    ) == 1;
}
/**
 * 生成随机登录令牌。
 *
 * 32 字节随机数编码为 64 个十六进制字符。
 * 随机数生成失败时返回空字符串。
 */
std::string randomToken(){
    unsigned char bytes[32];

    if(RAND_bytes(bytes,sizeof bytes)!=1){//生成32位随机数，写入bytes
        return {};
    }

    constexpr char hex[] = "0123456789abcdef";//十六进制字符表

    std::string result;
    result.reserve(64);

    for(unsigned char b : bytes){
        result += hex[b>>4];//右移四位，取出前四位
        result += hex[b&15];//保留后四位 15就是00001111
    }
    return result;
}

HttpResponse invalid(){
    return HttpResponse(
        400,
        "Bad Request",
        R"({"error":"invalid username or password format"})"
    );
}

HttpResponse unauth(){
    return HttpResponse(
        401,
        "Unauthorized",
        R"({"error":"unauthorized"})"
    );
}

HttpResponse internalError() {
    return HttpResponse(
        500,
        "Internal Server Error",
        R"({"error":"authentication service error"})"
    );
}

}//namespace

HttpResponse AuthHandler::registerUser(const HttpRequest& request){
    std::string username;
    std::string password;

    if(!credentials(request,username,password)){//检查格式
        return invalid();
    }

    if(users_.find(username)!=users_.end()){//用户存在
        return HttpResponse(
            409,
            "Conflict",
            R"({"error":"username already exists"})" 
        );
    }

    User user{};
    //为用户产生独立随即盐
    if(RAND_bytes(user.salt.data(),static_cast<int>(user.salt.size()))!=1 ||
        !derive(password,user.salt,user.hash)){
        return internalError();
    }

    users_.emplace(username,user);

    return HttpResponse(
        201,
        "Created",
        R"({"success":true})"
    );
}

HttpResponse AuthHandler::login(const HttpRequest& request){
    std::string username;
    std::string password;

    if(!credentials(request,username,password)){//检查格式
        return invalid();
    }

    const auto it = users_.find(username);

    if(it==users_.end()){//找不到
        return unauth();
    }

    std::array<unsigned char,32>hash{};

    if(!derive(password,it->second.salt,hash)){//利用注册保存的盐，重新计算摘要
        return internalError();
    }
    //比较注册时的摘要和当前你的摘要
    if(CRYPTO_memcmp(hash.data(),it->second.hash.data(),hash.size())!=0){
        return unauth();
    }

    const auto token = randomToken();//随机登录令牌

    if(token.empty()){
        return internalError();
    }

    const auto inserted = sessions_.emplace(token,LoginSession{
        username,
        std::chrono::steady_clock::now() + 
            std::chrono::minutes(30)
    });

    if(!inserted.second){
        return internalError();
    }

    HttpResponse response(
        200,
        "OK",
        R"({"success":true})"
    );

    response.setCookie("auth_session="+
        token + "; Path=/; HttpOnly; SameSite=Strict; Max-Age=1800");

    return response;
}
// 提取token
std::string AuthHandler::tokenFrom(const HttpRequest& request){
    const auto it = request.headers.find("cookie");

    if(it==request.headers.end()){
        return {};
    }
    
    std::string_view cookies(it->second);

    while(!cookies.empty()){
        const auto end = cookies.find(';');

        auto item = cookies.substr(0,end);

        const auto start = item.find_first_not_of(" \t");//去掉前面的空格和制表符

        if(start!=std::string_view::npos){
            item.remove_prefix(start);//跳过start个字符
        }

        constexpr std::string_view prefix = "auth_session=";

        if(item.substr(0,prefix.size())==prefix){
            return std::string(item.substr(prefix.size()));
        }

        if(end==std::string_view::npos){//没有auth_session=
            break;
        }
        cookies.remove_prefix(end+1);
    }
    return {};
}

bool AuthHandler::authenticatedUser(
    const HttpRequest& request,
    std::string& username
){
    const auto token = tokenFrom(request);

    const auto it = sessions_.find(token);

    if(it==sessions_.end()){
        return false;
    }

    if(std::chrono::steady_clock::now()>= it->second.expireAt){
        sessions_.erase(it);
        return false;
    }

    username = it->second.username;

    return true;
}

//查村cookie对应的登录用户
HttpResponse AuthHandler::me(const HttpRequest& request){
    std::string username;

    if(!authenticatedUser(request,username)){
        return unauth();
    }

    boost::json::object result;

    result["username"] = username;

    return HttpResponse(
        200,
        "OK",
        boost::json::serialize(result)
    );
}

HttpResponse AuthHandler::logout(const HttpRequest& request){
     sessions_.erase(tokenFrom(request));

    HttpResponse response(
        200,
        "OK",
        R"({"success":true})"
    );

    // 让客户端删除 Cookie。
    response.setCookie(
        "auth_session=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0"
    );

    return response;
}