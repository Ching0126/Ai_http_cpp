#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <messaging/ChatEventStore.h>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace{

std::string envOr(const char* name,const char* fallback){
    const char* value = std::getenv(name);

    return value && *value ? value : fallback;
}

int mqPort() {
    const auto text = envOr(
        "AI_MQ_PORT",
        "5672"
    );

    std::size_t used = 0;

    const int port = std::stoi(text, &used);

    if (
        used != text.size() ||
        port < 1 ||
        port > 65535
    ) {
        throw std::invalid_argument(
            "invalid AI_MQ_PORT"
        );
    }

    return port;

}
}//namespace 

int main(){
    try{
        //准备连接配置，登录RabbitMQ
        AmqpClient::Channel::OpenOpts options;
        options.host = envOr("AI_MQ_HOST","127.0.0.1");
        options.port = mqPort();
        options.vhost = envOr("AI_MQ_VHOST","/");
        options.auth = AmqpClient::Channel::OpenOpts::BasicAuth(envOr("AI_MQ_USER","guest"),envOr("AI_MQ_PASSWORD", "guest"));

        //确定队列，进行连接
        const auto queue = envOr("AI_MQ_QUEUE","ai_http_chat_events_v1");
        auto channel = AmqpClient::Channel::Open(options);
        channel->DeclareQueue(queue,false,true,false,false);

        //注册自己为消费者，：我要从 queue 这个队列消费消息。
        const auto tag = channel->BasicConsume(queue,"",true,false,false,1);

        std::cout<<"Waiting for chat events on"<<queue<<std::endl;
        
        const ChatEventStore store;

        for (;;) {
        AmqpClient::Envelope::ptr_t envelope;

        if (!channel->BasicConsumeMessage(tag,envelope,1000)) {
            continue;
        }

       bool inserted = false;
        try {
            // 新事件返回 true；已经处理过返回 false。
            inserted = store.save(envelope->Message()->Body());
        } catch (const InvalidChatEvent&) {
            // 错误格式不能靠重试修复。
            // false 表示不重新放回原队列。
            channel->BasicReject(envelope,false);

            std::cerr<< "invalid chat event discarded"<< std::endl;

            continue;
        }

        // 只有写入成功后才确认消息,重复时间也需要确认，不然就会一直投递
        channel->BasicAck(envelope);

        std::cout<< (inserted? "chat event saved": "duplicate chat event ignored")<< std::endl;
    }

    } catch (const std::exception&) {
        std::cerr
            << "chat event consumer failed; "
            << "unacknowledged messages may be redelivered"
            << std::endl;

        return 1;
    }
}