#include "session/ChatSessionManager.h"

#include <stdexcept>

std::string ChatSessionManager::create(const std::string& owner){//创建新会话
    if(nextId_ == 0){//一个循环了，会话够多了。
        throw std::overflow_error("session IDs exhausted");
    }

    const std::string id = std::to_string(nextId_);

    sessions_.emplace(id,ChatSession{id,owner,{}});

    ++nextId_;

    return id;
}

ChatSession* ChatSessionManager::find(const std::string& id,const std::string& owner){
    const auto it = sessions_.find(id);
    
    if(it==sessions_.end()||it->second.owner != owner){
        return nullptr;
    }
    return &it->second;
}

std::vector<std::string> ChatSessionManager::list(const std::string& owner) const{
    std::vector<std::string> ids;

    for(const auto& entry:sessions_){
        if(entry.second.owner==owner)
            ids.push_back(entry.first);
    }
    return ids;
}