#include "messaging/ChatEventPublisher.h"
#include <SimpleAmqpClient/SimpleAmqpClient.h>

#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {
std::string envOr(const char* name,const char* fallback) {
    const char* value = std::getenv(name);

    return value && *value ? value : fallback;
}
//读取并检查RabbitMQ端口
int mqPort(){
    const auto text = envOr("AI_MQ_PORT","5672");

    std::size_t used = 0;

    const int port = std::stoi(text,&used);

    if(used!=text.size()||port<1||port>65535){
        throw std::invalid_argument("invalid AI_MQ_PORT");
    }
    return port;
}
}//namespace 

bool ChatEventPublisher::enabled(){
    return envOr("AI_MQ_ENABLED","0") == "1";
}

void ChatEventPublisher::publish(const std::string& eventJson){
    AmqpClient::Channel::OpenOpts options;

    options.host = envOr("AI_MQ_HOST","127.0.0.1");

    options.port = mqPort();

    options.vhost = envOr("AI_MQ_VHOST","/");

    options.auth = AmqpClient::Channel::OpenOpts::BasicAuth(
        envOr("AI_MQ_USER","guest"),
        envOr("AI_MQ_PASSWORD","guest")
    );

    const auto queue = envOr("AI_MQ_QUEUE","ai_http_chat_events_v1");

    auto channel = AmqpClient::Channel::Open(options);

    channel->DeclareQueue(queue,false,true,false,false);

    auto message = AmqpClient::BasicMessage::Create(eventJson);

    message->DeliveryMode(AmqpClient::BasicMessage::dm_persistent);

    channel->BasicPublish("",queue,message);

}


