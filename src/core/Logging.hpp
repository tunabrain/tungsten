#ifndef LOGGING_HPP_
#define LOGGING_HPP_

#include <string>

namespace Tungsten {

void printProgressBar(int tick, int maxTicks);
void printTimestampedLog(const std::string &s);

}

#endif /* LOGGING_HPP_ */
