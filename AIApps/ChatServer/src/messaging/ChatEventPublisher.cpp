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

    const int port = std::stoi(text,&used);//字符串转整数

    if(used!=text.size()||port<1||port>65535){
        throw std::invalid_argument("invalid AI_MQ_PORT");
    }
    return port;
}
}//namespace 

bool ChatEventPublisher::enabled(){//检查是否启动事件发布，没有配置环境变量，就默认0 就是没启动
    return envOr("AI_MQ_ENABLED","0") == "1";
}

void ChatEventPublisher::publish(const std::string& eventJson){
    AmqpClient::Channel::OpenOpts options;

    options.host = envOr("AI_MQ_HOST","127.0.0.1");

    options.port = mqPort();

    options.vhost = envOr("AI_MQ_VHOST","/");//虚拟主机的名称

    options.auth = AmqpClient::Channel::OpenOpts::BasicAuth(//填写用户名和密码
        envOr("AI_MQ_USER","guest"),
        envOr("AI_MQ_PASSWORD","guest")
    );

    const auto queue = envOr("AI_MQ_QUEUE","ai_http_chat_events_v1");//读取队列名称

    auto channel = AmqpClient::Channel::Open(options);//打开连接和通道

    channel->DeclareQueue(
    queue,  // 队列名称
    false,  // passive：不是只检查队列是否已存在
    true,   // durable：队列声明为持久化
    false,  // exclusive：不是当前连接独占
    false   // auto_delete：不使用自动删除
    );

    auto message = AmqpClient::BasicMessage::Create(eventJson);//把字符串包装成消息

    message->DeliveryMode(AmqpClient::BasicMessage::dm_persistent);//设置持久化

    channel->BasicPublish(//发布消息
        "",      // 使用默认交换机
        queue,   // 路由键，这里使用队列名
        message  // 要发送的消息
    );


}