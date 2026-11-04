#pragma once

#include "http/HttpRequest.h"
#include "http/HttpResponse.h"

#include <array>
#include <chrono>
#include <map>
#include <string>
//注册和登录管理器
class AuthHandler{
public:
    //注册
    HttpResponse registerUser(const HttpRequest& request);
    //验证用户名和密码，成功后创建登录会话
    HttpResponse login(const HttpRequest& request);
    //销毁cookie对应的登录会话
    HttpResponse logout(const HttpRequest& request);
    //查询cookie对应的登录用户
    HttpResponse me(const HttpRequest& request);

    bool authenticatedUser(//根据cookie返回username
        const HttpRequest& request,
        std::string& username
    );
private:
    //保存密码所需的随即盐和摘要
    struct User{
        std::array<unsigned char,16> salt;
        std::array<unsigned char,32> hash;
    };
    //一个登录会话
    struct LoginSession{
        std::string username;
        std::chrono::steady_clock::time_point expireAt;//单调时钟，判断过期时间
    };

    static std::string tokenFrom(const HttpRequest& request);//提取登录令牌

    std::map<std::string,User> users_;

    std::map<std::string,LoginSession>sessions_;//从随机令牌到登录会话
};