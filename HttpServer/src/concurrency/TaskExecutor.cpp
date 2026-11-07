#include "concurrency/TaskExecutor.h"

#include <stdexcept>
#include <utility>

TaskExecutor::TaskExecutor(
    std::size_t workerCount,//工作线程数量
    std::size_t queueLimit//排队限制
):queueLimit_(queueLimit){
    if(workerCount ==0 || queueLimit ==0){
        throw std::invalid_argument("invalid executor size");
    }

    try{
        for(std::size_t i=0;i<workerCount;++i){
            workers_.emplace_back(&TaskExecutor::runWorker,this);//一个工作线程一个启动
        }
    }catch(...){
        stop();
        throw;
    }
}

TaskExecutor::~TaskExecutor(){
    stop();
}

bool TaskExecutor::trySubmit(std::function<void()> task){//加任务
    if(!task){
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(stopping_||tasks_.size()>=queueLimit_){
            return false;
        }

        tasks_.push(std::move(task));//提交任务
    }
    ready_.notify_one();

    return true;
}

void TaskExecutor::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }

    // 唤醒所有工作线程，让它们检查停止条件。
    ready_.notify_all();
    for (auto& worker : workers_) {//回收所有的线程
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void TaskExecutor::runWorker(){
    for(;;){
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);

            ready_.wait(lock,[this]{
                return stopping_ || !tasks_.empty();//返回true，就可以工作了
            });

            if(tasks_.empty()){//队列为空了，被别人取走了返回。
                return ;
            }

            task = std::move(tasks_.front());//存入任务
            tasks_.pop();
        }

        try{
            task();//执行任务
        }catch(...){

        }
    }
}