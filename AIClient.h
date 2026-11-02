#pragma once
#include <string>

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