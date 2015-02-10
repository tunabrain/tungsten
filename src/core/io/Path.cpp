#include "Path.hpp"
#include "FileIterables.hpp"
#include "FileUtils.hpp"

#ifdef _MSC_VER
#include <dirent/dirent.h>
#else
#include <sys/stat.h>
#include <dirent.h>
#endif
#include <locale>

namespace Tungsten {

typedef std::string::size_type SizeType;

#if _WIN32
static const char *separators = "/\\";
#else
static const char *separators = "/";
#endif

static bool isSeparator(char p)
{
#if _WIN32
    return p == '/' || p == '\\';
#else
    return p == '/';
#endif
}

static void ensureSeparator(std::string &s)
{
    if (s.empty() || isSeparator(s.back()))
        return;
    s += '/';
}

static SizeType findFilenamePos(const std::string &s)
{
    if (s.empty() || (isSeparator(s.back()) && s.size() == 1))
        return std::string::npos;
#if _WIN32
    if (s.back() == ':')
        return std::string::npos;
#endif

    SizeType n = s.find_last_of(separators, s.size() - 1 - isSeparator(s.back()));
#if _WIN32
    if (n == std::string::npos)
        n = s.find_last_of(':');
#endif

    if (n == std::string::npos)
        return 0;
    else
        return n + 1;
}

static SizeType findExtensionPos(const std::string &s)
{
    if (s.empty() || s.back() == '.' || isSeparator(s.back()))
        return std::string::npos;

    SizeType start = findFilenamePos(s);
    if (start == std::string::npos)
        return std::string::npos;

    SizeType n = s.find_last_of('.');
    if (n == std::string::npos || n < start)
        return std::string::npos;
    return n;
}

Path::Path(const std::string &workingDirectory, const std::string &path)
: _workingDirectory(workingDirectory),
  _path(path)
{
}

Path::Path(const std::string &path)
: _path(path)
{
}

Path::Path(const char *path)
: _path(path)
{
}

bool Path::testExtension(const Path &ext) const
{
    if (size() <= ext.size())
        return false;
    if (_path[size() - ext.size() - 1] != '.')
        return false;

    // Sinze extensions don't normally contain localized characters, this should work fine
    for (size_t i = 0; i < ext.size(); ++i)
        if (toupper(_path[size() - ext.size() + i]) != toupper(ext.path()[i]))
            return false;

    return true;
}

bool Path::isRootDirectory() const
{
    if (size() == 1 && isSeparator(_path[0]))
        return true;

#if _WIN32
    // Fully qualified drive paths (e.g. C:\)
    if (size() == 2 && _path[1] == ':')
        return true;
    if (size() == 3 && _path[1] == ':' && isSeparator(_path[2]))
        return true;

    // Network path
    if (size() == 2 && isSeparator(_path[0]) && isSeparator(_path[1]))
        return true;
#endif
    return false;
}

bool Path::isAbsolute() const
{
    return !isRelative();
}

bool Path::isRelative() const
{
    if (empty())
        return true;
    if (isSeparator(_path[0]))
        return false;

#if _WIN32
    // ShlWapi functions do not play well with forward slashes.
    // Do not replace this with PathIsRelative!

    if (size() >= 2) {
        // UNC path (e.g. \\foo\bar.txt)
        if (isSeparator(_path[0]) && isSeparator(_path[1]))
            return false;
        // We're ignoring drive relative paths (e.g. C:foo.txt) here,
        // since not even the WinAPI gets them right.
        if (_path[1] == ':')
            return false;
    }
#endif
    return true;
}

bool Path::isDirectory() const
{
    struct stat buffer;
    if (stat(absolutePath().c_str(), &buffer) == -1)
        return false;
    return S_ISDIR(buffer.st_mode);
}

bool Path::isFile() const
{
    struct stat buffer;
    if (stat(absolutePath().c_str(), &buffer) == -1)
        return false;
    return S_ISREG(buffer.st_mode);
}

bool Path::exists() const
{
    if(empty())
        return false;
    struct stat buffer;
    return (stat(absolutePath().c_str(), &buffer) == 0);
}

bool Path::empty() const
{
    return _path.empty();
}

size_t Path::size() const
{
    return _path.size();
}

void Path::freezeWorkingDirectory()
{
    _workingDirectory = FileUtils::getCurrentDir();
    Tungsten::ensureSeparator(_workingDirectory);
}

void Path::clearWorkingDirectory()
{
    _workingDirectory.clear();
}

std::string Path::absolutePath() const
{
    if (isAbsolute())
        return _path;
    else if (!_workingDirectory.empty())
        return _workingDirectory + _path;
    else {
        std::string dir = FileUtils::getCurrentDir();
        Tungsten::ensureSeparator(dir);
        return dir + _path;
    }
}

const std::string &Path::path() const
{
    return _path;
}

Path Path::extension() const
{
    SizeType extPos = findExtensionPos(_path);
    if (extPos != std::string::npos)
        return Path(_workingDirectory, _path.substr(extPos));
    else
        return Path(_workingDirectory, "");
}

Path Path::fileName() const
{
    SizeType startPos = findFilenamePos(_path);
    if (startPos == std::string::npos)
        return Path(_workingDirectory, "");

    SizeType endPos = _path.size();
    if (isSeparator(_path.back()))
        endPos--;

    return Path(_workingDirectory, _path.substr(startPos, endPos - startPos));
}

Path Path::baseName() const
{
    SizeType startPos = findFilenamePos(_path);
    if (startPos == std::string::npos)
        return Path(_workingDirectory, "");
    SizeType extPos = findExtensionPos(_path);
    if (extPos == std::string::npos) {
        extPos = _path.size();
        if (isSeparator(_path.back()))
            extPos--;
    }
    return Path(_workingDirectory, _path.substr(startPos, extPos - startPos));
}

Path Path::parent() const
{
    if (isRootDirectory())
        return Path(_workingDirectory, "");

    SizeType pos = findFilenamePos(_path);
    if (pos == std::string::npos || pos == 0)
        return Path(_workingDirectory, "");
    return Path(_workingDirectory, _path.substr(0, pos));
}

Path Path::stripParent() const
{
    if (isRootDirectory())
        return *this;

    SizeType pos = findFilenamePos(_path);
    if (pos == std::string::npos)
        return *this;
    return Path(_workingDirectory, _path.substr(pos));
}

Path Path::stripExtension() const
{
    SizeType extPos = findExtensionPos(_path);
    if (extPos == std::string::npos)
        return *this;
    return Path(_workingDirectory, _path.substr(0, extPos));
}

Path Path::setExtension(const Path &ext) const
{
    if (empty() || isSeparator(_path.back()) || isRootDirectory())
        return *this;
    else if (!ext.empty() && ext.path().front() != '.')
        return stripExtension() + "." + ext;
    else
        return stripExtension() + ext;
}

void Path::ensureSeparator()
{
    Tungsten::ensureSeparator(_path);
}

void Path::stripSeparator()
{
    if (size() == 1 && isSeparator(_path[0]))
        return;
#if _WIN32
    if (size() == 2 && isSeparator(_path[0]) && isSeparator(_path[1]))
        return;
#endif
    if (!empty() && isSeparator(_path.back()))
        _path.pop_back();
}

Path &Path::operator/=(const Path &o)
{
    return *this /= o.path();
}

Path &Path::operator+=(const Path &o)
{
    return *this += o.path();
}

Path &Path::operator/=(const std::string &o)
{
    ensureSeparator();
    return *this += o;
}

Path &Path::operator+=(const std::string &o)
{
    _path += o;
    return *this;
}

Path &Path::operator/=(const char *o)
{
    ensureSeparator();
    return *this += o;
}

Path &Path::operator+=(const char *o)
{
    _path += o;
    return *this;
}

Path Path::operator/(const Path &o) const
{
    return *this / o.path();
}

Path Path::operator+(const Path &o) const
{
    return *this + o.path();
}

Path Path::operator/(const std::string &o) const
{
    Path copy(*this);
    copy.ensureSeparator();
    copy += o;
    return std::move(copy);
}

Path Path::operator+(const std::string &o) const
{
    Path copy(*this);
    copy += o;

    return std::move(copy);
}

Path Path::operator/(const char *o) const
{
    Path copy(*this);
    copy.ensureSeparator();
    copy += o;
    return std::move(copy);
}

Path Path::operator+(const char *o) const
{
    Path copy(*this);
    copy += o;

    return std::move(copy);
}
FileIterator Path::begin() const
{
    return FileIterator(*this, false, false, Path());
}

FileIterator Path::end() const
{
    return FileIterator();
}

FileIterable Path::files(const Path &extensionFilter) const
{
    return FileIterable(*this, extensionFilter);
}

DirectoryIterable Path::directories() const
{
    return DirectoryIterable(*this);
}

}
