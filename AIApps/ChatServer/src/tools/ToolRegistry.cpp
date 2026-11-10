#include "tools/ToolRegistry.h"
#include "tools/McpWebSearch.h"

#include <ctime>
#include <stdexcept>
#include <utility>

namespace{

std::string getTime(const boost::json::object& args){
    if(!args.empty()){//不接受参数，如果接受了参数，就报错
        throw std::invalid_argument("get_time tasks no arguments");
    }

    const std::time_t now = std::time(nullptr);//获取当前时间

    std::tm local{};

    if(now==static_cast<std::time_t>(-1)||!localtime_r(&now,&local)){//localtime_r 将结果写到当前函数自己的 local 中。
        throw std::runtime_error("cannot read local time");
    }

    char buffer[64];

    if(std::strftime(buffer,sizeof buffer, "%Y-%m-%d %H:%M:%S %z",&local) == 0){
        throw std::runtime_error("cannot format local time");
    }

    return buffer;
}

}//namespace

ToolRegistry::ToolRegistry(){
    registerTool("get_time",getTime);

     // 配置了阿里云 MCP 地址和密钥时，才注册搜索工具。
    if (mcpSearchConfigured()) {
        registerTool("web_search", mcpWebSearch);
    }
}

void ToolRegistry::registerTool(const std::string& name,Tool tool){
    if(name.empty()||!tool){
        throw std::invalid_argument("invalid tool");
    }

    const auto result = tools_.emplace(name,std::move(tool));

    if(!result.second){
        throw std::invalid_argument("tool already exists");
    }
}

std::string ToolRegistry::invoke(const std::string& name,const boost::json::object& args)const{
     const auto it = tools_.find(name);

    if (it == tools_.end()) {
        throw std::invalid_argument(
            "unknown tool"
        );
    }

    return it->second(args);
}