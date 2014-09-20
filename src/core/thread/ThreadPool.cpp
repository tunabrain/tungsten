#include "ThreadPool.hpp"

namespace Tungsten {

ThreadPool::ThreadPool(uint32 threadCount)
: _threadCount(threadCount),
  _terminateFlag(false)
{
    startThreads();
}

ThreadPool::~ThreadPool()
{
    stop();
}

std::shared_ptr<TaskGroup> ThreadPool::acquireTask(uint32 &subTaskId)
{
    std::unique_lock<std::mutex> lock(_taskMutex);
    _taskCond.wait(lock, [this]{return _terminateFlag || !_tasks.empty();});
    if (_terminateFlag)
        return nullptr;
    std::shared_ptr<TaskGroup> task = _tasks.front();
    if (task->isAborting()) {
        _tasks.pop_front();
        return nullptr;
    }
    subTaskId = task->startSubTask();
    if (subTaskId == task->numSubTasks() - 1)
        _tasks.pop_front();
    return std::move(task);
}

void ThreadPool::runWorker(uint32 threadId)
{
    while (!_terminateFlag) {
        uint32 subTaskId;
        std::shared_ptr<TaskGroup> task = acquireTask(subTaskId);
        if (task)
            task->run(threadId, subTaskId);
    }
}

void ThreadPool::startThreads()
{
    _terminateFlag = false;
    for (uint32 i = 0; i < _threadCount; ++i)
        _workers.emplace_back(new std::thread(&ThreadPool::runWorker, this, _workers.size()));
}

void ThreadPool::reset()
{
    stop();
    _tasks.clear();
    startThreads();
}

void ThreadPool::stop()
{
    _terminateFlag = true;
    {
        std::unique_lock<std::mutex> lock(_taskMutex);
        _taskCond.notify_all();
    }
    while (!_workers.empty()) {
        _workers.back()->join();
        _workers.pop_back();
    }
}

std::shared_ptr<TaskGroup> ThreadPool::enqueue(TaskFunc func, int numSubtasks, Finisher finisher)
{
    std::shared_ptr<TaskGroup> task(std::make_shared<TaskGroup>(std::move(func),
            std::move(finisher), numSubtasks));

    {
        std::unique_lock<std::mutex> lock(_taskMutex);
        _tasks.emplace_back(task);
        if (numSubtasks == 1)
            _taskCond.notify_one();
        else
            _taskCond.notify_all();
    }

    return std::move(task);
}

}
