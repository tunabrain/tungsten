#include "OpenGL.hpp"

#include "BufferObject.hpp"

namespace Tungsten {

namespace GL {

static GLenum bufferTypes[] = {
    GL_ARRAY_BUFFER,
    GL_ELEMENT_ARRAY_BUFFER,
    GL_PIXEL_PACK_BUFFER,
    GL_PIXEL_UNPACK_BUFFER,
    GL_UNIFORM_BUFFER,
};

BufferObject::BufferObject(BufferType type)
: _type(type),
  _size(-1),
  _data(nullptr)
{
    _glType = bufferTypes[type];

    glf->glGenBuffers(1, &_glName);
}

BufferObject::BufferObject(BufferType type, GLsizei size)
: _type(type),
  _data(nullptr)
{
    _glType = bufferTypes[type];

    glf->glGenBuffers(1, &_glName);
    init(size);
}

BufferObject::~BufferObject()
{
    if (_glName)
        glf->glDeleteBuffers(1, &_glName);
}

BufferObject::BufferObject(BufferObject &&o)
{
    _type   = o._type;
    _glType = o._glType;
    _glName = o._glName;
    _size   = o._size;
    _data   = o._data;

    o._glName = 0;
}

BufferObject &BufferObject::operator=(BufferObject &&o)
{
    _type   = o._type;
    _glType = o._glType;
    _glName = o._glName;
    _size   = o._size;
    _data   = o._data;

    o._glName = 0;

    return *this;
}

void BufferObject::init(GLsizei size)
{
    _size = size;
    bind();
    glf->glBufferData(_glType, _size, nullptr, GL_STATIC_DRAW);
    unbind();
}

void BufferObject::map(int flags)
{
    if (flags & MAP_INVALIDATE)
        invalidate();

    GLuint flag = (flags & MAP_READ) ? ((flags & MAP_WRITE) ? GL_READ_WRITE : GL_READ_ONLY) : GL_WRITE_ONLY;

    _data = glf->glMapBuffer(_glType, flag);
}

void BufferObject::unmap()
{
    _data = nullptr;
    glf->glUnmapBuffer(_glType);
}

void BufferObject::copyData(void *data, GLsizei size)
{
    glf->glBufferData(_glType, size, data, GL_STATIC_DRAW);
}

void BufferObject::bind()
{
    glf->glBindBuffer(_glType, _glName);
}

void BufferObject::unbind()
{
    glf->glBindBuffer(_glType, 0);
}

void BufferObject::invalidate()
{
    glf->glBufferData(_glType, _size, nullptr, GL_STATIC_DRAW);
}

}

}
