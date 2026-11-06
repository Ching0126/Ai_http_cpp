#pragma once 

#include <array>
#include <map>
#include <string>
//用户密码记录，保存随机盐和密码摘要
struct UserRecord{
    std::array<unsigned char,16> salt{};
    std::array<unsigned char,32> hash{};
};

class UserRepository{
public:
    //从环境变量中读取存储模式以及数据库的配置
    UserRepository();
    //创建用户
    bool create(const std::string& username,const UserRecord& user);
    //查村用户
    bool find(const std::string& username,UserRecord& user)const;

private:
    bool mysqlMode_ = false;
    
    std::map<std::string,UserRecord> users_;
};
