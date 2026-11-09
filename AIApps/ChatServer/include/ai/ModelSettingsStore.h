#pragma once

#include <map>
#include <stdexcept>
#include <string>

class ModelSettingsError : public std::runtime_error{
public:
    using std::runtime_error::runtime_error;
};

class ModelSettingsStore{
public:
    ModelSettingsStore();

    bool find(const std::string& username , std::string& mode)const;

    void set(const std::string& username,const std::string& mode);

    void clear(const std::string& username);

private:
    bool mysqlMode_ = false ;

    std::map<std::string,std::string> modes_;
};