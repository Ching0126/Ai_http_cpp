#include "ai/ChatStrategy.h"
#include "ai/AIClient.h"

#include <exception>
#include <utility>

std::string MockChatStrategy::ask(const std::vector<ChatMessage>& history)const{
    if(history.empty()||history.back().role != "user"){
        throw AIRequestError("invalid chat history");
    }

    return "[模拟AI]收到你的消息" + history.back().content + "此前消息数： " + std::to_string(history.size()-1);
}

DeepSeekChatStrategy::DeepSeekChatStrategy(std::string key,std::string url,std::string model)
    :key_(std::move(key))
    ,url_(std::move(url))
    ,model_(std::move(model)){
}

std::string DeepSeekChatStrategy::ask(const std::vector<ChatMessage>& history)const{
    try{
        AIClient client(key_,url_,model_);

        return client.ask(history);
    }catch (const std::exception&){
        throw AIRequestError("AI request failed");
    }
}