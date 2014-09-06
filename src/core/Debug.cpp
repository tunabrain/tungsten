#include "Debug.hpp"

#include <iostream>

namespace Tungsten {

namespace DebugUtils {

void debugLog(const std::string &message)
{
    std::cout << message << std::endl;
}

}

}
