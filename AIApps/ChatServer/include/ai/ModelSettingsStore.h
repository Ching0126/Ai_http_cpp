#pragma once

#include <map>
#include <string>

class ModelSettingsStore{
public:
    bool find(const std::string& username , std::string& mode)const;

    void set(const std::string& username,const std::string& mode);

    void clear(const std::string& username);

private:
    std::map<std::string,std::string> modes_;
};