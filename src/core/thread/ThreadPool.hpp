#ifndef THREADPOOL_HPP_
#define THREADPOOL_HPP_

#include "TaskGroup.hpp"

#include "IntTypes.hpp"

#include <condition_variable>
#include <functional>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>
#include <deque>
#include <mutex>

namespace Tungsten {

class ThreadPool
{
    typedef std::function<void(uint32, uint32, uint32)> TaskFunc;
    typedef std::function<void()> Finisher;

    uint32 _threadCount;
    std::vector<std::unique_ptr<std::thread>> _workers;
    std::atomic<bool> _terminateFlag;

    std::deque<std::shared_ptr<TaskGroup>> _tasks;
    std::mutex _taskMutex;
    std::condition_variable _taskCond;

    std::shared_ptr<TaskGroup> acquireTask(uint32 &subTaskId);
    void runWorker(uint32 threadId);
    void startThreads();

public:
    ThreadPool(uint32 threadCount);
    ~ThreadPool();

    void reset();
    void stop();

    std::shared_ptr<TaskGroup> enqueue(TaskFunc func, int numSubtasks = 1,
            Finisher finisher = Finisher());

    uint32 threadCount() const
    {
        return _threadCount;
    }
};

}

#endif /* THREADPOOL_HPP_ */
