#ifndef STRINGUTILS_HPP_
#define STRINGUTILS_HPP_

#include <string>

namespace Tungsten {

namespace StringUtils {

double parseDuration(const std::string &str);
std::string durationToString(double secs);

}

}

#endif /* STRINGUTILS_HPP_ */
