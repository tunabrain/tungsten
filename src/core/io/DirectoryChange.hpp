#ifndef DIRECTORYCHANGE_HPP_
#define DIRECTORYCHANGE_HPP_

#include "FileUtils.hpp"
#include "Path.hpp"

namespace Tungsten {

class DirectoryChange
{
    Path _previousDir;

public:
    DirectoryChange(const Path &path)
    {
        if (!path.empty()) {
            _previousDir = FileUtils::getCurrentDir();
            FileUtils::changeCurrentDir(path);
        }
    }

    ~DirectoryChange()
    {
        if (!_previousDir.empty())
            FileUtils::changeCurrentDir(_previousDir);
    }
};

}

#endif /* DIRECTORYCHANGE_HPP_ */
