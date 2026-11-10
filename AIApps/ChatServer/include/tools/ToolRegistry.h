#pragma once

#include <boost/json.hpp>

#include <functional>
#include <map>
#include <string>

class ToolRegistry{
public:
    using Tool = std::function<std::string(const boost::json::object&)>;
    //创建注册表
    ToolRegistry();
    //注册一个工具
    void registerTool(const std::string& name,Tool tool);
    //按名称调用工具
    std::string invoke(const std::string& name,const boost::json::object& args)const;

private:
    std::map<std::string,Tool> tools_;
};