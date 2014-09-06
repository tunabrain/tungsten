#ifndef THREADPOOL_HPP_
#define THREADPOOL_HPP_

#include <condition_variable>
#include <exception>
#include <stdexcept>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>
#include <deque>
#include <mutex>

namespace Tungsten {

class Task
{
    std::function<void(uint32)> _func;

    std::mutex _waitMutex;
    std::condition_variable _waitCond;
    std::exception_ptr _exceptionPtr;
    std::atomic<bool> _done;

public:
    Task(std::function<void(uint32)> func)
    : _func(std::move(func)),
      _done(false)
    {
    }

    void wait()
    {
        std::unique_lock<std::mutex> lock(_waitMutex);
        _waitCond.wait(lock, [this]{return _done == true;});

        if (_exceptionPtr)
            std::rethrow_exception(_exceptionPtr);
    }

    void run(uint32 id)
    {
        try {
            _func(id);
        } catch (...) {
            _exceptionPtr = std::current_exception();
        }
        _done = true;
        _waitCond.notify_all();
    }
};

class ThreadPool
{
    int _numThreads;
    std::vector<std::unique_ptr<std::thread>> _workers;
    std::atomic<bool> _terminateFlag;

    std::deque<std::shared_ptr<Task>> _tasks;
    std::mutex _taskMutex;
    std::condition_variable _taskCond;

    std::shared_ptr<Task> acquireTask()
    {
        std::unique_lock<std::mutex> lock(_taskMutex);
        _taskCond.wait(lock, [this]{return _terminateFlag || !_tasks.empty();});
        if (_terminateFlag)
            return nullptr;
        std::shared_ptr<Task> task = std::move(_tasks.front());
        _tasks.pop_front();
        return std::move(task);
    }

    void runWorker(uint32 id)
    {
        while (!_terminateFlag) {
            std::shared_ptr<Task> task = acquireTask();
            if (task)
                task->run(id);
        }
    }

    void startThreads()
    {
        for (int i = 0; i < _numThreads; ++i)
            _workers.emplace_back(new std::thread(&ThreadPool::runWorker, this, _workers.size()));
    }

public:
    ThreadPool(int numThreads)
    : _numThreads(numThreads),
      _terminateFlag(false)
    {
        startThreads();
    }

    ~ThreadPool()
    {
        stop();
    }

    void reset()
    {
        stop();
        _tasks.clear();
        startThreads();
    }

    void stop()
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

    std::shared_ptr<Task> enqueue(std::function<void(uint32)> func)
    {
        std::shared_ptr<Task> task(std::make_shared<Task>(std::move(func)));
        {
            std::unique_lock<std::mutex> lock(_taskMutex);
            _tasks.emplace_back(task);
            _taskCond.notify_one();
        }
        return std::move(task);
    }
};

}

#endif /* THREADPOOL_HPP_ */
