#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <exception>

#include "Debug.hpp"

#include "extern/tinyformat/tinyformat.hpp"

namespace Tungsten
{

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
//
//void DebugUtils::debugFail(const char *file, int line, const char *format, ...) {
//    printf("PROGRAM FAILURE:  %s:%d:  ", file, line);
//    va_list args;
//    va_start(args, format);
//    vprintf(format, args);
//    printf("\n");
//    va_end(args);
//    fflush(stdout);
//    exit(EXIT_FAILURE);
//}

void DebugUtils::debug(const std::string &message)
{
    printf("%s\n", message.c_str());
    fflush(stdout);
}
//
//void DebugUtils::fail(const std::string &message)
//{
//  printf("%s\n", message.c_str());
//  exit(1);
//}

}
