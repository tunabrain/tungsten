#ifndef ZIPENTRY_HPP_
#define ZIPENTRY_HPP_

#include "Path.hpp"

#include "IntTypes.hpp"

#include <vector>

namespace Tungsten {

struct ZipEntry
{
    Path name;
    Path fullPath;
    uint32 size;
    bool isDirectory;
    int archiveIndex;
    std::vector<int> contents;
};

}



#endif /* ZIPENTRY_HPP_ */
