#ifndef THREADUTILS_HPP_
#define THREADUTILS_HPP_

#include "IntTypes.hpp"

#include <functional>

namespace Tungsten {

class ThreadPool;

namespace ThreadUtils {

extern ThreadPool *pool;

uint32 idealThreadCount();
void startThreads(int numThreads);

void parallelFor(uint32 start, uint32 end, uint32 partitions, std::function<void(uint32)> func);

}

}

#endif /* THREADUTILS_HPP_ */
