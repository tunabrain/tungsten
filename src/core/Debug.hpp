#ifndef DEBUG_HPP_
#define DEBUG_HPP_

#include <stdexcept>

#include <tinyformat/tinyformat.hpp>

namespace Tungsten {

namespace DebugUtils
{

void debugLog(const std::string &message);

}

// Can be enabled for a little bit more speed, but generally not recommended
#ifndef NO_DEBUG_MACROS
#define DEBUG_BEGIN
#define DEBUG_END
#else
#define DEBUG_BEGIN if (false) {
#define DEBUG_END }
#endif

# define FAIL(...) throw std::runtime_error(tfm::format("PROGRAM FAILURE in %s:%d: " FIRST(__VA_ARGS__), __FILE__, __LINE__ REST(__VA_ARGS__)))
# define DBG(...) do { DEBUG_BEGIN DebugUtils::debugLog(tfm::format("%s:%d: " FIRST(__VA_ARGS__), __FILE__, __LINE__ REST(__VA_ARGS__))); DEBUG_END } while(false)
# define ASSERT(EXP, ...) do { \
    DEBUG_BEGIN \
    if (!bool(EXP)) \
        throw std::runtime_error(tfm::format("ASSERTION FAILURE in %s:%d (" #EXP "): " FIRST(__VA_ARGS__), __FILE__, __LINE__ REST(__VA_ARGS__))); \
    DEBUG_END \
    } while(false);


/* See http://stackoverflow.com/questions/5588855/standard-alternative-to-gccs-va-args-trick */
/* expands to the first argument */
#define FIRST(...) FIRST_HELPER(__VA_ARGS__, throwaway)
#define FIRST_HELPER(first, ...) first

/*
 * if there's only one argument, expands to nothing.  if there is more
 * than one argument, expands to a comma followed by everything but
 * the first argument.  only supports up to 9 arguments but can be
 * trivially expanded.
 */
#define REST(...) REST_HELPER(NUM(__VA_ARGS__), __VA_ARGS__)
#define REST_HELPER(qty, ...) REST_HELPER2(qty, __VA_ARGS__)
#define REST_HELPER2(qty, ...) REST_HELPER_##qty(__VA_ARGS__)
#define REST_HELPER_ONE(first)
#define REST_HELPER_TWOORMORE(first, ...) , __VA_ARGS__
#define NUM(...) \
    SELECT_10TH(__VA_ARGS__, TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE,\
                TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE, ONE, throwaway)
#define SELECT_10TH(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, ...) a10

}

#endif /* DEBUG_HPP_ */
