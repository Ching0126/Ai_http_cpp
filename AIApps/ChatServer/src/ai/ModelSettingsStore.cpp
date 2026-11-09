#include "ai/ModelSettingsStore.h"

bool ModelSettingsStore::find(const std::string& username,std::string& mode)const{
    const auto it = modes_.find(username);

    if(it==modes_.end()){
        return false;
    }

    mode = it->second;
    return true;
}

void ModelSettingsStore::set(const std::string& username,const std::string& mode){
    modes_[username] = mode;
}

void ModelSettingsStore::clear(const std::string& username){
    modes_.erase(username);
}