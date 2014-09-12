#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <exception>

#include "Debug.hpp"

#include "extern/tinyformat/tinyformat.hpp"

namespace Tungsten {

static const char *levelStr[] = {"WARN", "INFO", "DEBUG"};

void DebugUtils::debugLog(const char *module, DebugLevel level, const char *fmt, ...)
{
    va_list argp;
    va_start(argp, fmt);
    printf("%s | %-10s | ", levelStr[level], module);
    vprintf(fmt, argp);
    printf("\n");
    va_end(argp);
    fflush(stdout);
}

void DebugUtils::debug(const std::string &message)
{
    printf("%s\n", message.c_str());
    fflush(stdout);
}

}
