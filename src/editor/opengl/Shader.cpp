#include "OpenGL.hpp"

#include "Shader.hpp"
#include "Debug.hpp"

#include <tinyformat/tinyformat.hpp>
#include <iostream>

namespace Tungsten {

namespace GL {

Shader::Shader()
: _program(0)
{
}

Shader::Shader(const Path &folder, const Path &preamble, const Path &vertex, const Path &geometry,
        const Path &fragment, int numOutputs)
: _program(0)
{
    if (!fragment.empty()) {
        ShaderObject frag(ShaderType::Fragment);
        if (!preamble.empty())
            frag.addFile(folder/preamble);
        frag.addFile(folder/fragment);
        frag.compile();
        addObject(std::move(frag));
    }
    if (!vertex.empty()) {
        ShaderObject vert(ShaderType::Vertex);
        if (!preamble.empty())
            vert.addFile(folder/preamble);
        vert.addFile(folder/vertex);
        vert.compile();
        addObject(std::move(vert));
    }
    if (!geometry.empty()) {
        ShaderObject geom(ShaderType::Geometry);
        if (!preamble.empty())
            geom.addFile(folder/preamble);
        geom.addFile(folder/geometry);
        geom.compile();
        addObject(std::move(geom));
    }
    for (int i = 0; i < numOutputs; ++i)
        addOutput(tfm::format("FragColor%d", i));

    link();
}

Shader::~Shader()
{
    if (_program)
        glDeleteProgram(_program);
}

Shader::Shader(Shader &&o)
{
    _program = o._program;
    _shaders = std::move(o._shaders);
    _outputs = std::move(o._outputs);
    _uniformLocations = std::move(o._uniformLocations);

    o._program = 0;
}

Shader &Shader::operator=(Shader &&o)
{
    _program = o._program;
    _shaders = std::move(o._shaders);
    _outputs = std::move(o._outputs);
    _uniformLocations = std::move(o._uniformLocations);

    o._program = 0;

    return *this;
}

void Shader::addObject(ShaderType type, const Path &path)
{
    _shaders.emplace_back(type, path);
}

void Shader::addObject(ShaderObject object)
{
    _shaders.emplace_back(std::move(object));
}

void Shader::addOutput(std::string name)
{
    _outputs.emplace_back(std::move(name));
}

void Shader::link()
{
    if (_program)
        glDeleteProgram(_program);

    _program = glCreateProgram();

    for (const ShaderObject &o : _shaders)
        glAttachShader(_program, o.glName());

    for (size_t i = 0; i < _outputs.size(); i++)
        glBindFragDataLocation(_program, i, _outputs[i].c_str());

    glLinkProgram(_program);

    check();
}

void Shader::bind()
{
    glUseProgram(_program);
}

GLint Shader::uniform(const std::string &name)
{
    auto iter = _uniformLocations.find(name);
    if (iter == _uniformLocations.end())
        iter = _uniformLocations.insert(std::make_pair(name, glGetUniformLocation(_program, name.c_str()))).first;
    return iter->second;
}

void Shader::check()
{
    int status, length;
    glGetProgramiv(_program, GL_LINK_STATUS, &status);
    glGetProgramiv(_program, GL_INFO_LOG_LENGTH, &length);

    std::unique_ptr<GLchar[]> log;
    if (length > 1) {
        log.reset(new GLchar[length]);
        glGetProgramInfoLog(_program, length, nullptr, log.get());
    }

    if (status != GL_TRUE) {
        std::cout << "Unable to link shader program:\n";
        std::cout << log.get() << std::endl;
        std::exit(0);
    } else if (length > 1) {
        std::cout << "Note: Shader program linking completed with message: \n" << log.get() << std::endl;
    }
}

void Shader::uniformI(const std::string &name, int i)
{
    glUniform1i(uniform(name), i);
}

void Shader::uniformI(const std::string &name, int i1, int i2)
{
    glUniform2i(uniform(name), i1, i2);
}

void Shader::uniformI(const std::string &name, int i1, int i2, int i3)
{
    glUniform3i(uniform(name), i1, i2, i3);
}

void Shader::uniformI(const std::string &name, int i1, int i2, int i3, int i4)
{
    glUniform4i(uniform(name), i1, i2, i3, i4);
}

void Shader::uniformF(const std::string &name, float f)
{
    glUniform1f(uniform(name), f);
}
void Shader::uniformF(const std::string &name, float f1, float f2)
{
    glUniform2f(uniform(name), f1, f2);
}

void Shader::uniformF(const std::string &name, float f1, float f2, float f3)
{
    glUniform3f(uniform(name), f1, f2, f3);
}

void Shader::uniformF(const std::string &name, float f1, float f2, float f3, float f4)
{
    glUniform4f(uniform(name), f1, f2, f3, f4);
}

void Shader::uniformF(const std::string &name, const Vec3f &v)
{
    glUniform3f(uniform(name), v.x(), v.y(), v.z());
}

void Shader::uniformF(const std::string &name, const Vec4f &v)
{
    glUniform4f(uniform(name), v.x(), v.y(), v.z(), v.w());
}

void Shader::uniformMat(const std::string &name, const Mat4f &m, bool transpose)
{
    glUniformMatrix4fv(uniform(name), 1, transpose, m.data());
}

}

}
