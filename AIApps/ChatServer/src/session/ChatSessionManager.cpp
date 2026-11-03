#include "session/ChatSessionManager.h"

#include <stdexcept>

std::string ChatSessionManager::create(){
    if(nextId_ == 0){
        throw std::overflow_error("session IDs exhausted");
    }

    const std::string id = std::to_string(nextId_);

    sessions_.emplace(id,ChatSession{id,{}});

    ++nextId_;

    return id;
}

ChatSession* ChatSessionManager::find(const std::string& id){
    const auto it = sessions_.find(id);
    
    if(it==sessions_.end()){
        return nullptr;
    }
    return &it->second;
}

std::vector<std::string> ChatSessionManager::list() const{
    std::vector<std::string> ids;

    for(const auto& entry:sessions_){
        ids.push_back(entry.first);
    }
    return ids;
}