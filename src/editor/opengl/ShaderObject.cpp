#include "ShaderObject.hpp"

#include "io/FileUtils.hpp"

#include <tinyformat/tinyformat.hpp>
#include <iostream>
#include <sstream>

namespace Tungsten {

namespace GL {

GLenum shaderTypeToGl(ShaderType type)
{
    switch (type) {
    case ShaderType::Vertex:   return GL_VERTEX_SHADER;
    case ShaderType::Fragment: return GL_FRAGMENT_SHADER;
    case ShaderType::Geometry: return GL_GEOMETRY_SHADER;
    }
    return 0;
}

ShaderObject::ShaderObject(ShaderType type)
: _type(type),
  _glName(0)
{
}

ShaderObject::ShaderObject(ShaderType type, const Path &path)
: _type(type),
  _glName(0)
{
    addFile(path);
    compile();
}

ShaderObject::~ShaderObject()
{
    if (_glName)
        glf->glDeleteShader(_glName);
}

ShaderObject::ShaderObject(ShaderObject &&o)
{
    _type    = o._type;
    _glName  = o._glName;
    _sources = std::move(o._sources);

    o._glName = 0;
}

ShaderObject &ShaderObject::operator=(ShaderObject &&o)
{
    _type    = o._type;
    _glName  = o._glName;
    _sources = std::move(o._sources);

    o._glName = 0;

    return *this;
}

void ShaderObject::addFile(const Path &path)
{
    _sources.emplace_back(ShaderSource {
        path,
        FileUtils::loadText(path)
    });
}

void ShaderObject::compile()
{
    std::vector<const GLchar *> sources;
    for (const ShaderSource &s : _sources)
        sources.push_back(s.source.c_str());

    if (_glName)
        glf->glDeleteShader(_glName);

    _glName = glf->glCreateShader(shaderTypeToGl(_type));
    glf->glShaderSource(_glName, _sources.size(), &sources[0], nullptr);
    glf->glCompileShader(_glName);

    check();
}

void ShaderObject::check()
{
    int status, length;
    glf->glGetShaderiv(_glName, GL_COMPILE_STATUS, &status);
    glf->glGetShaderiv(_glName, GL_INFO_LOG_LENGTH, &length);

    std::unique_ptr<GLchar[]> log;
    if (length > 1) {
        log.reset(new GLchar[length]);
        glf->glGetShaderInfoLog(_glName, length, nullptr, log.get());
    }

    if (status != GL_TRUE) {
        int srcLength;
        glf->glGetShaderiv(_glName, GL_SHADER_SOURCE_LENGTH, &srcLength);

        std::unique_ptr<GLchar[]> src(new GLchar[srcLength]);
        glf->glGetShaderSource(_glName, srcLength, nullptr, src.get());

        std::istringstream stream(src.get());
        std::string line;
        int lineNumber = 1;

        std::cout << "---------------------------\n";
        while (std::getline(stream, line))
            tfm::printf("%4d | %s\n", lineNumber++, line);
        std::cout << "---------------------------\n";
        std::cout << "Unable to compile shader:\n";
        std::cout << log.get() << std::endl;
        std::exit(0);
    } else if (length > 1) {
        std::cout << "Note: Shader compilation completed with message: \n" << log.get() << std::endl;
    }
}

}

}
