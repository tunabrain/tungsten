#ifndef RENDER_SHADER_OBJECT_HPP_
#define RENDER_SHADER_OBJECT_HPP_

#include "OpenGL.hpp"

#include "io/Path.hpp"

#include <string>
#include <vector>

namespace Tungsten {

namespace GL {

enum class ShaderType
{
    Vertex,
    Geometry,
    Fragment,
};

class ShaderObject
{
    struct ShaderSource
    {
        Path path;
        std::string source;
    };

    ShaderType _type;
    GLuint _glName;

    std::vector<ShaderSource> _sources;

    void check();

public:
    ShaderObject(ShaderType type);
    ShaderObject(ShaderType type, const Path &path);
    ~ShaderObject();

    ShaderObject(ShaderObject &&o);
    ShaderObject &operator=(ShaderObject &&o);

    ShaderObject(const ShaderObject &) = delete;
    ShaderObject &operator=(const ShaderObject &) = delete;

    void addFile(const Path &path);
    void compile();

    ShaderType type() const
    {
        return _type;
    }

    GLuint glName() const
    {
        return _glName;
    }
};

}

}

#endif /* SHADER_OBJECT_H_ */
