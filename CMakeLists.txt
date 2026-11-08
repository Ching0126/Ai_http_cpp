cmake_minimum_required(VERSION 3.16)
project(ai_http_cpp LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(Boost 1.75 REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)

set(
    SIMPLE_AMQP_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/.deps"
    CACHE PATH
    "SimpleAmqpClient installation prefix"
)

# 兼容之前配置过但值为空的构建目录。
if(SIMPLE_AMQP_ROOT STREQUAL "")
    set(SIMPLE_AMQP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.deps" CACHE PATH
        "SimpleAmqpClient installation prefix" FORCE)
endif()

find_path(
    SIMPLE_AMQP_INCLUDE_DIR
    SimpleAmqpClient/SimpleAmqpClient.h
    HINTS "${SIMPLE_AMQP_ROOT}/include"
    REQUIRED
)

find_library(
    SIMPLE_AMQP_LIBRARY
    NAMES SimpleAmqpClient
    HINTS "${SIMPLE_AMQP_ROOT}/lib"
    REQUIRED
)


find_path(MUDUO_INCLUDE_DIR muduo/net/TcpServer.h REQUIRED)

find_library(MUDUO_NET muduo_net REQUIRED)
find_library(MUDUO_BASE muduo_base REQUIRED)


find_path(
    MYSQLCPP_INCLUDE_DIR
    mysql_driver.h
    REQUIRED
)

find_library(
    MYSQLCPP_LIBRARY
    mysqlcppconn
    REQUIRED
)

# 聊天事件发布器。
add_library(chat_events STATIC
    AIApps/ChatServer/src/messaging/ChatEventPublisher.cpp
)

target_include_directories(chat_events PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/AIApps/ChatServer/include
    ${SIMPLE_AMQP_INCLUDE_DIR}
)

target_link_libraries(chat_events PUBLIC
    ${SIMPLE_AMQP_LIBRARY}
)

# 独立消费者程序。
add_executable(chat_event_consumer
    AIApps/ChatServer/tools/chat_event_consumer.cpp
    AIApps/ChatServer/src/messaging/ChatEventStore.cpp
    AIApps/ChatServer/src/storage/DbConnectionPool.cpp
)

target_include_directories(chat_event_consumer PRIVATE
    ${MYSQLCPP_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(chat_event_consumer PRIVATE
     chat_events
    ${MYSQLCPP_LIBRARY}
    Threads::Threads
)


# 框架单独编译成库；PUBLIC 表示使用该库的目标也需要这些依赖。
add_library(http_framework STATIC
    HttpServer/src/http/HttpContext.cpp
    HttpServer/src/http/HttpResponse.cpp
    HttpServer/src/http/HttpServer.cpp
    HttpServer/src/router/Router.cpp
    HttpServer/src/middleware/MiddlewareChain.cpp
    HttpServer/src/middleware/RequestLogMiddleware.cpp
    HttpServer/src/middleware/CorsMiddleware.cpp
    HttpServer/src/concurrency/TaskExecutor.cpp
)

target_include_directories(http_framework PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/HttpServer/include
    ${MUDUO_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
)


target_link_libraries(http_framework PUBLIC
    ${MUDUO_NET}
    ${MUDUO_BASE}
    Threads::Threads
)

# 聊天应用使用 HTTP 框架和 libcurl。
add_executable(ai_http_server
    AIApps/ChatServer/src/main.cpp
    AIApps/ChatServer/src/ChatServer.cpp
    AIApps/ChatServer/src/handlers/ChatHandler.cpp
    AIApps/ChatServer/src/ai/AIClient.cpp
    AIApps/ChatServer/src/session/ChatSessionManager.cpp
    AIApps/ChatServer/src/auth/AuthHandler.cpp
    AIApps/ChatServer/src/handlers/StaticFileHandler.cpp
    AIApps/ChatServer/src/storage/UserRepository.cpp
    AIApps/ChatServer/src/storage/DbConnectionPool.cpp
)


target_include_directories(ai_http_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/AIApps/ChatServer/include
    ${MYSQLCPP_INCLUDE_DIR}
)
# 链接库文件
target_link_libraries(ai_http_server PRIVATE
    http_framework
    chat_events
    CURL::libcurl
    OpenSSL::Crypto
    ${MYSQLCPP_LIBRARY}
)

target_compile_definitions(ai_http_server PRIVATE
    CHAT_RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/AIApps/ChatServer/resource"
)

# 用于验证重复事件去重的工具。
add_executable(replay_chat_event
    AIApps/ChatServer/tools/replay_chat_event.cpp
)

target_link_libraries(replay_chat_event PRIVATE
    chat_events
)
