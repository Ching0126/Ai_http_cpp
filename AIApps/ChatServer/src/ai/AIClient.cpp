#include "ai/AIClient.h"

#include <boost/json.hpp>
#include <curl/curl.h>

#include <memory>
#include <stdexcept>
#include <utility>

// 1. 构造请求 JSON。
// 2. 配置并执行 libcurl 请求。
// 3. 判断网络和 HTTP 状态。
// 4. 解析返回 JSON 中的 choices[0].message.content。
namespace{
//
// 限制上游响应大小，防止无限累积。
constexpr std::size_t maxResponseBytes = 1024*1024;
/**
 * libcurl 接收到响应数据时调用。
 *
 * @param data 本次收到的数据。
 * @param size 每个数据项的字节数。
 * @param count 数据项数量。
 * @param userData 指向用于保存响应正文的 string。
 *
 * 返回实际接收的字节数；返回 0 表示写入失败。
 */
std::size_t writeBody(
    char* data,
    std::size_t size,
    std::size_t count,
    void* userData
){
    auto* output = static_cast<std::string*>(userData);

    if(size!=0 && count>maxResponseBytes/size){
        return 0;
    }

    const std::size_t bytes = size * count;

    if(output->size()>maxResponseBytes-bytes){//如果把本次收到的数据追加进去，响应内容会不会超过允许的最大大小。
        return 0;
    }

    try{
        output->append(data,bytes);
    }catch(...){
        return 0; 
    }
    return bytes;
}
/**
 * 管理 libcurl 请求头链表。
 *
 * 对象销毁时自动释放链表。
 */
class Headers{
public:
    ~Headers(){
        curl_slist_free_all(list_);
    }
    //添加一个请求头
    void add(const std::string& value){
        auto* next = curl_slist_append(//添加后返回的是整个链表的头指针
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
        url_.rfind("https://", 0) != 0 ||//https://在第0位置
        key_.find_first_of("\r\n")!=std::string::npos){//找到\r\n
        throw std::invalid_argument("invalid AI configuration");
    }
}
// POST /chat/completions HTTP/1.1
// Host: api.deepseek.com
// Content-Type: application/json
// Authorization: Bearer 你的API密钥
// Content-Length: 正文的字节数

// {
//   "model": "deepseek-flash",
//   "messages": [
//     {
//       "role": "user",
//       "content": "什么是 HTTP？"
//     }
//   ],
//   "stream": false,
//   "thinking": {
//     "type": "disabled"
//   }
// }
std::string AIClient::ask(
    const std::vector<ChatMessage>& history
)const {
    // 1. 构造发送给 DeepSeek 的 JSON。
    boost::json::array messages;

    for(const auto& item :history){
        boost::json::object message;

        message["role"] = item.role;
        message["content"] = item.content;

        messages.push_back(message);
    }

    boost::json::object payload;

    payload["model"]=model_;
    payload["messages"]=messages;

    payload["stream"]=false;

    payload["thinking"]={
        {"type","disabled"}
    };

    const std::string requestBody = 
        boost::json::serialize(payload);

    // 2. 创建 curl 句柄，离开作用域时自动释放。
    using CurlPtr = std::unique_ptr<CURL,decltype(&curl_easy_cleanup)>;

    CurlPtr curl(curl_easy_init(),&curl_easy_cleanup);

    if(!curl){
        throw std::runtime_error("curl initialization failed");
    }
    
    Headers headers;//自定义类

    headers.add("Content-Type: application/json");
    headers.add("Authorization: Bearer "+key_);

    std::string responseBody;

    //3. 配置请求地址、请求头和请求正文。
    //curl_easy_setopt(curl.get(), 选项, 选项对应的值);
    //3.1 请求格式
    curl_easy_setopt(curl.get(),CURLOPT_URL,url_.c_str());//设置请求地址

    curl_easy_setopt(curl.get(),CURLOPT_HTTPHEADER,headers.get());//设置请求头

    curl_easy_setopt(curl.get(),CURLOPT_POSTFIELDS,requestBody.c_str());//设置post请求正文

    curl_easy_setopt(curl.get(),CURLOPT_POSTFIELDSIZE,static_cast<long>(requestBody.size()));//设置正文字节

    //3.2收到回复之后
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION,writeBody);//收到响应正文时，调用 writeBody 函数处理。

    curl_easy_setopt(curl.get(),CURLOPT_WRITEDATA,&responseBody);//调用 writeBody 时，把 responseBody 的地址作为 userData 传进去。

    //3.3超时等待
    curl_easy_setopt(curl.get(),CURLOPT_CONNECTTIMEOUT,5L);//连接超时5s

    curl_easy_setopt(curl.get(),CURLOPT_TIMEOUT,45L);//整个请求过程超时

    curl_easy_setopt(curl.get(),CURLOPT_NOSIGNAL,1L);//信号设置

    const CURLcode code = curl_easy_perform(curl.get());//发送

    if(code != CURLE_OK){
        throw std::runtime_error("AI netwwork request failed");
    }
    // 4. 网络请求完成不代表上游一定返回成功。
    long status = 0;

    if(curl_easy_getinfo(curl.get(),CURLINFO_RESPONSE_CODE,&status)!= CURLE_OK||
        status <200 ||
        status >=300){
        throw std::runtime_error("AI provider returned an error");
    }
     // 5. 解析 DeepSeek 的 JSON 响应。

    // {
    // "choices": [
    //     {
    //     "message": {
    //         "role": "assistant",
    //         "content": "这里是模型的回答"
    //     }
    //     }
    // ]
    // }

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