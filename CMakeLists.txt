cmake_minimum_required(VERSION 3.16)

project(ai_http_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖包
find_package(Threads REQUIRED)
find_package(Boost 1.75 REQUIRED)

find_path(MUDUO_INCLUDE_DIR
        muduo/net/TcpServer.h
        REQUIRED
)
# 查找网络库
find_library(MUDUO_NET
            muduo_net
            REQUIRED)

find_library(MUDUO_BASE
    muduo_base
    REQUIRED
)
# 定义生成的程序目标名和源文件
add_executable(ai_http_server
                main.cpp
                ChatServer.cpp)
# 给程序添加头文件搜索目录
target_include_directories(ai_http_server PRIVATE
                            ${MUDUO_INCLUDE_DIR}
                            ${Boost_INCLUDE_DIRS}
)
# 给程序链接库
target_link_libraries(ai_http_server PRIVATE
                    ${MUDUO_NET}
                    ${MUDUO_BASE}
                    Threads::Threads
)   
