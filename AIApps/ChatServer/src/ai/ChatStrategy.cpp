#include "ai/ChatStrategy.h"
#include "ai/AIClient.h"

#include <exception>
#include <utility>

std::string MockChatStrategy::ask(const std::vector<ChatMessage>& history,const ToolRegistry& tools)const{
    if(history.empty()||history.back().role != "user"){
        throw AIRequestError("invalid chat history");
    }
    // 仅用于离线演示：模拟模型选择时间工具。
    // 这里使用的是中文问号，输入需要与字符串完全一致。
    if (history.back().content == "现在几点？") {
        try {
            return "[模拟模型使用工具] 当前服务器时间：" +tools.invoke("get_time", {});
        } catch (const std::exception&) {
            throw AIRequestError("tool invocation failed");
        }
    }

    return "[模拟AI]收到你的消息" +history.back().content +"此前消息数： " +std::to_string(history.size() - 1);
}

DeepSeekChatStrategy::DeepSeekChatStrategy(std::string key,std::string url,std::string model)
    :key_(std::move(key))
    ,url_(std::move(url))
    ,model_(std::move(model)){
}

std::string DeepSeekChatStrategy::ask(const std::vector<ChatMessage>& history,const ToolRegistry& tools)const{
    try{
        AIClient client(key_,url_,model_);

        return client.ask(history,tools);
    }catch (const std::exception&){
        throw AIRequestError("AI request failed");
    }
}