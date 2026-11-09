#include "ai/ChatStrategyFactory.h"

#include <cstdlib>
#include <memory>
#include <string>

namespace{

std::string envOr(const char* name,const char* fallback){
    const char* value = std::getenv(name);

    return value && *value ? value : fallback;
}

}//namespace

std::unique_ptr<ChatStrategy> ChatStrategyFactory::create(const std::string& mode){
    if(mode == "mock"){
        return std::make_unique<MockChatStrategy>();
    }

    if(mode == "deepseek"){
        const auto key = envOr("DEEPSEEK_API_KEY","");

        if (key.empty()) {
            throw AIConfigError("API key is not configured");
        }

        const auto url = envOr("DEEPSEEK_API_URL","https://api.deepseek.com/chat/completions");

        const auto model = envOr("DEEPSEEK_MODEL","deepseek-flash");

        return std::make_unique<DeepSeekChatStrategy>(key,url,model);
    }

    throw AIConfigError("invalid AI_MODE");
}