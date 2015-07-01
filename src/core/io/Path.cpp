#include "Path.hpp"
#include "FileIterables.hpp"
#include "FileUtils.hpp"

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

Path::Path(const Path &workingDirectory, const std::string &path)
: _workingDirectory(workingDirectory.absolute().ensureSeparator().asString()),
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
        if (toupper(_path[size() - ext.size() + i]) != toupper(ext.asString()[i]))
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
    return FileUtils::isDirectory(*this);
}

bool Path::isFile() const
{
    return FileUtils::isFile(*this);
}

bool Path::exists() const
{
    return FileUtils::exists(*this);
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
    _workingDirectory = FileUtils::getCurrentDir().ensureSeparator().asString();
}

void Path::clearWorkingDirectory()
{
    _workingDirectory.clear();
}

void Path::setWorkingDirectory(const Path &dir)
{
    _workingDirectory = dir.absolute().ensureSeparator().asString();
}

const std::string &Path::asString() const
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
    else if (!ext.empty() && ext.asString().front() != '.')
        return stripExtension() + "." + ext;
    else
        return stripExtension() + ext;
}

Path Path::absolute() const
{
    if (isAbsolute())
        return *this;
    else if (!_workingDirectory.empty())
        return Path(_workingDirectory, _workingDirectory + _path);
    else
        return FileUtils::getCurrentDir()/_path;
}

Path Path::normalizeSeparators() const
{
    Path path(*this);
#if _WIN32
    for (char &c : path._workingDirectory)
        if (c == '\\')
            c = '/';
    for (char &c : path._path)
        if (c == '\\')
            c = '/';
#endif
    return std::move(path);
}

Path Path::nativeSeparators() const
{
    Path path(*this);
#if _WIN32
    for (char &c : path._workingDirectory)
        if (c == '/')
            c = '\\';
    for (char &c : path._path)
        if (c == '/')
            c = '\\';
#endif
    return std::move(path);
}

Path Path::ensureSeparator() const
{
    Path result(*this);
    if (!_path.empty() && !isSeparator(_path.back()))
        result._path += '/';
    return std::move(result);
}

Path Path::stripSeparator() const
{
    Path result(*this);

    if (size() == 1 && isSeparator(_path[0]))
        return std::move(result);
#if _WIN32
    if (size() == 2 && isSeparator(_path[0]) && isSeparator(_path[1]))
        return std::move(result);
#endif
    if (!empty() && isSeparator(_path.back()))
        result._path.pop_back();

    return std::move(result);
}

Path Path::normalize() const
{
    std::string base = absolute().normalizeSeparators().stripSeparator().asString();

    std::string prefix;
    int offset = 0;

#if _WIN32
    if (base.size() >= 2 && isSeparator(base[0]) && isSeparator(base[1])) {
        prefix = "//";
        offset = 2;
    } else if (base.size() >= 2 && base[1] == ':') {
        prefix = base.substr(0, 2);
        if (base.size() > 2 && isSeparator(base[2])) {
            prefix += '/';
            offset = 3;
        } else {
            offset = 2;
        }
    }
#endif
    if (prefix.empty() && !base.empty() && isSeparator(base[0])) {
        prefix = '/';
        offset = 1;
    }

    std::vector<std::string> components;
    do {
        SizeType next = base.find_first_of('/', offset);
        if (next == std::string::npos)
            next = base.size();

        std::string component(base.substr(offset, next - offset));
        components.emplace_back(std::move(component));

        offset = int(next + 1);
    } while (offset < int(base.size()));

    std::vector<std::string> resultComponents;
    for (size_t i = 0; i < components.size(); ++i) {
        if (components[i].empty() || components[i] == ".")
            continue;
        if (components[i] == "..") {
            if (!resultComponents.empty())
                resultComponents.pop_back();
        } else {
            resultComponents.emplace_back(std::move(components[i]));
        }
    }

    std::string result = prefix;
    for (size_t i = 0; i < resultComponents.size(); ++i) {
        if (i)
            result += '/';
        result += resultComponents[i];
    }

    return Path(_workingDirectory, result);
}

Path &Path::operator/=(const Path &o)
{
    return *this /= o.asString();
}

Path &Path::operator+=(const Path &o)
{
    return *this += o.asString();
}

Path &Path::operator/=(const std::string &o)
{
    if (!empty() && !isSeparator(_path.back()))
        _path += '/';
    return *this += o;
}

Path &Path::operator+=(const std::string &o)
{
    _path += o;
    return *this;
}

Path &Path::operator/=(const char *o)
{
    if (!empty() && !isSeparator(_path.back()))
        _path += '/';
    return *this += o;
}

Path &Path::operator+=(const char *o)
{
    _path += o;
    return *this;
}

Path Path::operator/(const Path &o) const
{
    return *this / o.asString();
}

Path Path::operator+(const Path &o) const
{
    return *this + o.asString();
}

Path Path::operator/(const std::string &o) const
{
    if (o.empty())
        return *this;
    else
        return ensureSeparator() + o;
}

Path Path::operator+(const std::string &o) const
{
    Path copy(*this);
    copy += o;

    return std::move(copy);
}

Path Path::operator/(const char *o) const
{
    return ensureSeparator() + o;
}

Path Path::operator+(const char *o) const
{
    Path copy(*this);
    copy += o;

    return std::move(copy);
}

bool Path::operator==(const Path &o) const
{
    return _path == o._path;
}

bool Path::operator!=(const Path &o) const
{
    return _path != o._path;
}

bool Path::operator<(const Path &o) const
{
    return _path < o._path;
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

RecursiveIterable Path::recursive() const
{
    return RecursiveIterable(*this);
}

}
