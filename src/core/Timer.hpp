#ifndef TIMER_HPP_
#define TIMER_HPP_

#include "IntTypes.hpp"

#include "extern/tinyformat/tinyformat.hpp"

#include <chrono>

//#if defined(__GNUC__) && (defined(__WIN32) || defined(__WIN64))
//#define USE_RDTSC 1
//#endif

namespace Tungsten {

class Timer
{
#if USE_RDTSC
    typedef union
    {
        int64 longWord;
        int32 shortWord[2];
    } tsc_counter;

    tsc_counter _start, _stop;
#else
    std::chrono::time_point<std::chrono::high_resolution_clock> _start, _stop;
#endif
public:
    Timer()
    {
        start();
    }

#if USE_RDTSC
    void measure(tsc_counter &tsc) const
    {
#if defined(__ia64__)
    __asm__ __volatile__ ("mov %0=ar.itc" : "=r" (tsc.longWord) );
#else
    __asm__ __volatile__ ("rdtsc" : "=a" (tsc.shortWord[0]), "=d"(tsc.shortWord[1]));
    __asm__ __volatile__ ("cpuid" : : "a" (0) : "bx", "cx", "dx" );
#endif
    }
#endif

    void start()
    {
#if USE_RDTSC
        measure(_start);
#else
        _start = std::chrono::high_resolution_clock::now();
#endif
    }

    void stop()
    {
#if USE_RDTSC
        measure(_stop);
#else
        _stop = std::chrono::high_resolution_clock::now();
#endif
    }

    void bench(const std::string &s)
    {
        stop();
        std::cout << tfm::format("%s: %f s\n", s, elapsed());
    }

    double elapsed() const
    {
#if USE_RDTSC
        return double(_stop.longWord - _start.longWord)*(1.0/(2.4*1e9));
#else
        return std::chrono::duration<double>(_stop - _start).count();
#endif
    }
};

}

#endif /* TIMER_HPP_ */
