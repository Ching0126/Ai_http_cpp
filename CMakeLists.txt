cmake_minimum_required(VERSION 3.16)
project(ai_http_cpp LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(Boost 1.75 REQUIRED)
find_package(CURL REQUIRED)
find_path(MUDUO_INCLUDE_DIR muduo/net/TcpServer.h REQUIRED)
find_library(MUDUO_NET muduo_net REQUIRED)
find_library(MUDUO_BASE muduo_base REQUIRED)
find_package(OpenSSL REQUIRED)
# 框架单独编译成库；PUBLIC 表示使用该库的目标也需要这些依赖。
add_library(http_framework STATIC
    HttpServer/src/http/HttpContext.cpp
    HttpServer/src/http/HttpResponse.cpp
    HttpServer/src/http/HttpServer.cpp
    HttpServer/src/router/Router.cpp
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
)
target_include_directories(ai_http_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/AIApps/ChatServer/include
)
target_link_libraries(ai_http_server PRIVATE
    http_framework
    CURL::libcurl
    OpenSSL::Crypto
)

target_compile_definitions(ai_http_server PRIVATE
    CHAT_RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/AIApps/ChatServer/resource"
)

