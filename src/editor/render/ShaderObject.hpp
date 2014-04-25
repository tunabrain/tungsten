#ifndef RENDER_SHADER_OBJECT_HPP_
#define RENDER_SHADER_OBJECT_HPP_

#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

namespace Tungsten
{

namespace GL
{

#define MAX_SOURCES 16

enum ShaderType
{
    INVALID_SHADER  = -1,
    VERTEX_SHADER   = GL_VERTEX_SHADER,
    GEOMETRY_SHADER = GL_GEOMETRY_SHADER,
    FRAGMENT_SHADER = GL_FRAGMENT_SHADER,
    COMPUTE_SHADER  = GL_COMPUTE_SHADER
};

struct ShaderSource
{
    const char *file;
    char *src;
#ifdef _WIN32
    FILETIME timestamp;
#else
    time_t timestamp;
#endif
};

class ShaderObject {
    ShaderType _type;
    GLuint _name;

    int _sourceCount;
    ShaderSource _sources[MAX_SOURCES];

    void loadFile(ShaderSource *s, const char *path);
    void check();

public:
    ShaderObject() : _type(INVALID_SHADER), _name(0), _sourceCount(0) {}

    void addFile(const char *path);
    int refresh();
    void compile(ShaderType type);

    ShaderType type() const
    {
        return _type;
    }

    GLuint name() const
    {
        return _name;
    }

    int sourceCount() const
    {
        return _sourceCount;
    }

    const ShaderSource &source(int i) const
    {
        return _sources[i];
    }
};

}

}

#endif /* SHADER_OBJECT_H_ */
