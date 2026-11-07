#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class TaskExecutor{
public:

    TaskExecutor(std::size_t workerCount,std::size_t queueLimit);

    ~TaskExecutor();

    TaskExecutor(const TaskExecutor&)=delete;
    TaskExecutor& operator=(const TaskExecutor&)=delete;

    bool trySubmit(std::function<void()> task);

    void stop();

private:
    //工作线程循环：等待任务，取出任务，执行任务
    void runWorker();

    std::size_t queueLimit_;

    std::mutex mutex_;

    std::condition_variable ready_;

    std::queue<std::function<void()>> tasks_;//任务队列
    std::vector<std::thread> workers_;//工作线程池

    bool stopping_ = false;
};