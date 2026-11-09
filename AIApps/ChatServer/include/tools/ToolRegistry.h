#pragma once

#include <boost/json.hpp>

#include <functional>
#include <map>
#include <string>

class ToolRegistry{
public:
    using Tool = std::function<std::string(const boost::json::object&)>;

    ToolRegistry();

    void registerTool(const std::string& name,Tool tool);

    std::string invoke(const std::string& name,const boost::json::object& args)const;

private:
    std::map<std::string,Tool> tools_;
};