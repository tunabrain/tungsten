#ifndef TIMER_HPP_
#define TIMER_HPP_

#include "IntTypes.hpp"

#include <tinyformat/tinyformat.hpp>

#include <chrono>
#if _WIN32
#include <windows.h>
#endif

namespace Tungsten {

// std::chrono::high_resolution_clock has disappointing accuracy on windows
// On windows, we use the WinAPI high performance counter instead
class Timer
{
#if _WIN32
    LARGE_INTEGER _pfFrequency;
    LARGE_INTEGER _start, _stop;
#else
    std::chrono::time_point<std::chrono::high_resolution_clock> _start, _stop;
#endif
public:
    Timer()
    {
#if _WIN32
        QueryPerformanceFrequency(&_pfFrequency);
#endif
        start();
    }

    void start()
    {
#if _WIN32
        QueryPerformanceCounter(&_start);
#else
        _start = std::chrono::high_resolution_clock::now();
#endif
    }

    void stop()
    {
#if _WIN32
        QueryPerformanceCounter(&_stop);
#else
        _stop = std::chrono::high_resolution_clock::now();
#endif
    }

    void bench(const std::string &s)
    {
        stop();
        std::cout << tfm::format("%s: %f s", s, elapsed()) << std::endl;
    }

    double elapsed() const
    {
#if _WIN32
        return double(_stop.QuadPart - _start.QuadPart)/double(_pfFrequency.QuadPart);
#else
        return std::chrono::duration<double>(_stop - _start).count();
#endif
    }
};

}

#endif /* TIMER_HPP_ */
