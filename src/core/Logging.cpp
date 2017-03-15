#include <tinyformat/tinyformat.hpp>

#include <iostream>
#include <string>
#include <ctime>

namespace Tungsten {

void printProgressBar(int tick, int maxTicks)
{
    const int numCols = 80;
    const int numTicks = numCols - 19;

    int numFilledPrev = ((tick - 1)*numTicks)/maxTicks;
    int numFilledCur  = ((tick + 0)*numTicks)/maxTicks;
    if (tick != 0 && numFilledPrev == numFilledCur)
        return;

    std::string bar(numTicks, ' ');
    for (int i = 0; i < numFilledCur; ++i)
        bar[i] = '#';

    std::cout << tfm::format("\r           [%s] %3d%%", bar, (100*tick)/maxTicks);
    std::cout.flush();

    if (tick == maxTicks) {
        std::cout << '\r' << std::string(numCols, ' ') << '\r';
        std::cout.flush();
    }
}

void printTimestampedLog(const std::string &s)
{
    std::time_t t = std::time(NULL);
    char mbstr[100];
    std::strftime(mbstr, sizeof(mbstr), "[%H:%M:%S] ", std::localtime(&t));
    std::cout << mbstr << s << '\n';
    std::cout.flush();
}

}
