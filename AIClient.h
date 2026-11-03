#pragma once
#include <string>
//：向 DeepSeek 发出一个问题并取得回答。
class AIClient{
public:
    AIClient(
        std::string key,
        std::string url,
        std::string model
    );

    std::string ask(const std::string& question) const;
private:
    std::string key_;
    std::string url_;
    std::string model_;
};