#ifndef DIRECTORYCHANGE_HPP_
#define DIRECTORYCHANGE_HPP_

#include "FileUtils.hpp"

namespace Tungsten {

class DirectoryChange
{
    std::string _previousDir;

public:
    DirectoryChange(const std::string &path)
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
