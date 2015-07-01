#include "StringUtils.hpp"

#include "IntTypes.hpp"

#include <tinyformat/tinyformat.hpp>
#include <cstdlib>
#include <sstream>
#include <cctype>

namespace Tungsten {

namespace StringUtils {

double parseDuration(const std::string &str)
{
    double result = 0.0f;

    const char *buf = &str[0];
    const char *end = &str[str.size()];
    while (buf < end) {
        char *next;
        double number = std::strtod(buf, &next);
        if (next == buf)
            break;

        while (next < end && std::isspace(*next))
            next++;

        double unit = 60.0; // Default to minutes
        if (std::tolower(*next) == 'm' && std::tolower(*(next + 1)) == 's')
            unit = 0.001;
        else if (std::tolower(*next) == 's')
            unit = 1.0;
        else if (std::tolower(*next) == 'm')
            unit = 60.0;
        else if (std::tolower(*next) == 'h')
            unit = 60.0*60.0;
        else if (std::tolower(*next) == 'd')
            unit = 24.0*60.0*60.0;
        result += number*unit;

        while (next < end && std::isalpha(*next))
            next++;

        buf = next;
    }

    return result;
}

std::string durationToString(double secs)
{
    uint64 seconds = uint64(secs);
    uint64 minutes = seconds/60;
    uint64 hours = minutes/60;
    uint64 days = hours/24;
    double fraction = secs - seconds;

    std::stringstream ss;

    if (days)    ss << tfm::format("%dd ", days);
    if (hours)   ss << tfm::format("%dh ", hours % 24);
    if (minutes) ss << tfm::format("%dm ", minutes % 60);
    if (seconds) ss << tfm::format("%ds %dms", seconds % 60, uint64(fraction*1000.0f) % 1000);
    else ss << secs << "s";

    return std::move(ss.str());
}

}

}
