#include <SimpleAmqpClient/SimpleAmqpClient.h>

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
        AmqpClient::Channel::OpenOpts options;

        options.host = envOr("AI_MQ_HOST","127.0.0.1");

        options.port = mqPort();

        options.vhost = envOr("AI_MQ_VHOST","/");

        options.auth = AmqpClient::Channel::OpenOpts::BasicAuth(envOr("AI_MQ_USER","guest"),envOr("AI_MQ_PASSWORD", "guest"));

        const auto queue = envOr("AI_MQ_QUEUE","ai_http_chat_events_v1");

        auto channel = AmqpClient::Channel::Open(options);

        channel->DeclareQueue(queue,false,true,false,false);

        const auto tag = channel->BasicConsume(queue,"",true,false,false,1);

        std::cout<<"Waiting for chat events on"<<queue<<std::endl;

        for(;;){
            AmqpClient::Envelope::ptr_t envelope;

            if(!channel->BasicConsumeMessage(tag,envelope,1000)){
                continue;
            }

            std::cout<<envelope->Message()->Body()<<std::endl;

            if(!std::cout){
                throw std::runtime_error("cannot write event output");
            }

            channel->BasicAck(envelope);
        }
    }catch(const std::exception&){
        std::cerr<<"chat event consumer failed"<<std::endl;
        return 1;
    }
}