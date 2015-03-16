#ifndef RENDER_SHADER_HPP_
#define RENDER_SHADER_HPP_

#include "OpenGL.hpp"

#include "ShaderObject.hpp"

#include "math/Vec.hpp"
#include "math/Mat4f.hpp"

#include <unordered_map>

namespace Tungsten {

namespace GL {

class Shader
{
    struct UniformLocation
    {
        uint32 hash;
        int location;
    };

    GLuint _program;

    std::vector<ShaderObject> _shaders;
    std::vector<std::string> _outputs;
    std::unordered_map<std::string, GLint> _uniformLocations;

    void check();

public:
    Shader();
    Shader(const Path &folder, const Path &preamble, const Path &vertex, const Path &geometry,
            const Path &fragment, int numOutputs);
    ~Shader();

    Shader(Shader &&o);
    Shader &operator=(Shader &&o);

    Shader(const Shader &) = delete;
    Shader &operator=(const Shader &) = delete;

    void addObject(ShaderType type, const Path &path);
    void addObject(ShaderObject object);
    void addOutput(std::string name);

    void link();
    void bind();

    GLint uniform(const std::string &name);
    void uniformI(const std::string &name, int f1);
    void uniformI(const std::string &name, int f1, int f2);
    void uniformI(const std::string &name, int f1, int f2, int f3);
    void uniformI(const std::string &name, int f1, int f2, int f3, int f4);
    void uniformF(const std::string &name, float f1);
    void uniformF(const std::string &name, float f1, float f2);
    void uniformF(const std::string &name, float f1, float f2, float f3);
    void uniformF(const std::string &name, float f1, float f2, float f3, float f4);
    void uniformF(const std::string &name, const Vec3f &v);
    void uniformF(const std::string &name, const Vec4f &v);
    void uniformMat(const std::string &name, const Mat4f &v, bool transpose);

    GLint program() const
    {
        return _program;
    }
};

}

}

#endif /* RENDER_SHADER_HPP_ */
