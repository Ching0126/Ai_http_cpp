#include "messaging/ChatEventPublisher.h"

#include <algorithm>
#include <exception>
#include <iostream>
#include <string>

/**
 * 将同一个事件连续发送两次，用于验证消费者去重。
 *
 * 参数是 32 位小写十六进制 eventId。
 */
int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr
            << "usage: replay_chat_event "
            << "<32-lowercase-hex-id>"
            << std::endl;

        return 1;
    }

    const std::string id = argv[1];

    const bool valid = std::all_of(
        id.begin(),
        id.end(),
        [](unsigned char c) {
            return
                (c >= '0' && c <= '9') ||
                (c >= 'a' && c <= 'f');
        }
    );

    if (id.size() != 32 || !valid) {
        std::cerr
            << "invalid event id"
            << std::endl;

        return 1;
    }

    const std::string event =
        "{\"eventId\":\"" + id +
        "\",\"type\":\"chat.completed\","
        "\"username\":\"replay_user\","
        "\"sessionId\":\"1\"}";

    try {
        // 故意发送两次完全相同的事件。
        ChatEventPublisher::publish(event);
        ChatEventPublisher::publish(event);

        std::cout
            << "same event sent twice"
            << std::endl;
    } catch (const std::exception&) {
        std::cerr
            << "event replay failed"
            << std::endl;

        return 1;
    }

    return 0;
}