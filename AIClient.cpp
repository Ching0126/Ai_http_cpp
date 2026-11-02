#include "AIClient.h"

#include <boost/json.hpp>
#include <curl/curl.h>

#include <memory>
#include <stdexcept>
#include <utility>

namespace{

constexpr std::size_t maxResponseBytes = 1024*1024;

std::size_t writeBody(
    char* data,
    std::size_t size,
    std::size_t count,
    void* userData
){
    auto* output = 
        static_cast<std::string*>(userData);

    if(size!=0 && count>maxResponseBytes/size){
        return 0;
    }

    const std::size_t bytes = size * count;

    if(output->size()>maxResponseBytes-bytes){
        return 0;
    }

    try{
        output->append(data,bytes);
    }catch(...){
        return 0; 
    }
    return bytes;
}

class Headers{
public:
    ~Headers(){
        curl_slist_free_all(list_);
    }

    void add(const std::string& value){
        auto* next = curl_slist_append(
            list_,
            value.c_str()
        );

        if(!next){
            throw std::runtime_error("cannot allocate curl headers");
        }
        list_ = next;
    }

    curl_slist* get()const{
        return list_;
    }
private:
    curl_slist* list_ = nullptr;
};
}//namespace

AIClient::AIClient(
    std::string key,
    std::string url,
    std::string model
)
    :key_(std::move(key))
    ,url_(std::move(url))
    ,model_(std::move(model)){
    if(key_.empty()||
        model_.empty()||
        url_.rfind("https://", 0) != 0 ||
        key_.find_first_of("\r\n")!=std::string::npos){
        throw std::invalid_argument("invalid AI configuration");
    }
}

std::string AIClient::ask(
    const std::string& question
)const {
    boost::json::object message;

    message["role"] = "user";
    message["content"] = question;

    boost::json::array messages;
    messages.push_back(message);

    boost::json::object payload;

    payload["model"]=model_;
    payload["messages"]=messages;

    payload["stream"]=false;

    payload["thinking"]={
        {"type","disabled"}
    };

    const std::string requestBody = 
        boost::json::serialize(payload);

    using CurlPtr = std::unique_ptr<CURL,decltype(&curl_easy_cleanup)>;

    CurlPtr curl(curl_easy_init(),&curl_easy_cleanup);

    if(!curl){
        throw std::runtime_error("curl initialization failed");
    }
    
    Headers headers;

    headers.add("Content-Type: application/json");
    headers.add("Authorization: Bearer "+key_);

    std::string responseBody;

    curl_easy_setopt(curl.get(),CURLOPT_URL,url_.c_str());

    curl_easy_setopt(curl.get(),CURLOPT_HTTPHEADER,headers.get());

    curl_easy_setopt(curl.get(),CURLOPT_POSTFIELDS,requestBody.c_str());

    curl_easy_setopt(curl.get(),CURLOPT_POSTFIELDSIZE,static_cast<long>(requestBody.size()));

    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION,writeBody);

    curl_easy_setopt(curl.get(),CURLOPT_WRITEDATA,&responseBody);

    curl_easy_setopt(curl.get(),CURLOPT_CONNECTTIMEOUT,5L);

    curl_easy_setopt(curl.get(),CURLOPT_TIMEOUT,45L);

    curl_easy_setopt(curl.get(),CURLOPT_NOSIGNAL,1L);

    const CURLcode code = curl_easy_perform(curl.get());

    if(code != CURLE_OK){
        throw std::runtime_error("AI netwwork request failed");
    }

    long status = 0;

    if(curl_easy_getinfo(curl.get(),CURLINFO_RESPONSE_CODE,&status)!= CURLE_OK||
        status <200 ||
        status >=300){
        throw std::runtime_error("AI provider returned an error");
    }

    boost::system::error_code ec;

    const auto result = boost::json::parse(responseBody,ec);

    if(ec || !result.is_object()){
        throw std::runtime_error("invalid AI response");
    }

    const auto* choices = result.as_object().if_contains("choices");

    if(!choices ||
        !choices->is_array()||
        choices->as_array().empty()||
        !choices->as_array().front().is_object()){
        throw std::runtime_error("invalid AI choices");
    }

    const auto* messageResult = 
        choices->as_array()
            .front()
            .as_object()
            .if_contains("message");
    
    if(!messageResult ||
        !messageResult->is_object()){
        throw std::runtime_error("invalid AI message");
    }

    const auto* content = 
        messageResult->as_object()
            .if_contains("content");

    if(!content||
        !content->is_string()||
        content->as_string().empty()){
        throw std::runtime_error("empty AI answer");
    }

    return std::string(content->as_string().c_str(),content->as_string().size());

}