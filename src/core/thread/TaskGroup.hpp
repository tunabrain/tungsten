#ifndef TASKGROUP_HPP_
#define TASKGROUP_HPP_

#include "IntTypes.hpp"

#include <condition_variable>
#include <functional>
#include <exception>
#include <stdexcept>
#include <atomic>
#include <thread>
#include <mutex>

namespace Tungsten {

class TaskGroup
{
    typedef std::function<void(uint32, uint32, uint32)> TaskFunc;
    typedef std::function<void()> Finisher;

    TaskFunc _func;
    Finisher _finisher;

    std::exception_ptr _exceptionPtr;
    std::atomic<uint32> _startedSubTasks;
    std::atomic<uint32> _finishedSubTasks;
    uint32 _numSubTasks;

    std::mutex _waitMutex;
    std::condition_variable _waitCond;
    std::atomic<bool> _done, _abort;

    void finish()
    {
        if (_finisher && !_abort)
            _finisher();

        std::unique_lock<std::mutex> lock(_waitMutex);
        _done = true;
        _waitCond.notify_all();
    }

public:
    TaskGroup(TaskFunc func, Finisher finisher, uint32 numSubTasks)
    : _func(std::move(func)),
      _finisher(std::move(finisher)),
      _startedSubTasks(0),
      _finishedSubTasks(0),
      _numSubTasks(numSubTasks),
      _done(false),
      _abort(false)
    {
    }

    void run(uint32 threadId, uint32 taskId)
    {
        try {
            _func(taskId, _numSubTasks, threadId);
        } catch (...) {
            _exceptionPtr = std::current_exception();
        }

        uint32 num = ++_finishedSubTasks;
        if (num == _numSubTasks || (_abort && num == _startedSubTasks))
            finish();
    }

    void wait()
    {
        std::unique_lock<std::mutex> lock(_waitMutex);
        _waitCond.wait(lock, [this]{return _done == true;});

        if (_exceptionPtr)
            std::rethrow_exception(_exceptionPtr);
    }

    void abort()
    {
        std::unique_lock<std::mutex> lock(_waitMutex);
        _abort = true;
        _done = _done || _startedSubTasks == 0;
        _waitCond.notify_all();
    }

    bool isAborting() const
    {
        return _abort;
    }

    bool isDone() const
    {
        return _done;
    }

    uint32 startSubTask()
    {
        return _startedSubTasks++;
    }

    uint32 numSubTasks() const
    {
        return _numSubTasks;
    }
};

}

#endif /* TASKGROUP_HPP_ */
