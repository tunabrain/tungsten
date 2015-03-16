#ifndef RENDER_BUFFEROBJECT_HPP_
#define RENDER_BUFFEROBJECT_HPP_

#include "OpenGL.hpp"

namespace Tungsten {

namespace GL {

enum BufferType
{
    ARRAY_BUFFER,
    ELEMENT_ARRAY_BUFFER,
    PIXEL_PACK_BUFFER,
    PIXEL_UNPACK_BUFFER,
    UNIFORM_BUFFER
};

enum MapFlags
{
    MAP_READ             = (1 << 0),
    MAP_WRITE            = (1 << 1),
    MAP_INVALIDATE       = (1 << 2),
};

class BufferObject
{
    BufferType _type;
    GLenum _glType;
    GLuint _glName;
    GLsizei _size;

    void *_data;

public:
    BufferObject(BufferType type);
    BufferObject(BufferType type, GLsizei size);
    virtual ~BufferObject();

    BufferObject(BufferObject &&o);
    BufferObject &operator=(BufferObject &&o);

    BufferObject(const BufferObject &) = delete;
    BufferObject &operator=(const BufferObject &) = delete;

    void init(GLsizei size);

    void map(int flags = MAP_READ | MAP_WRITE | MAP_INVALIDATE);
    template<typename T>
    T *map(int flags = MAP_READ | MAP_WRITE | MAP_INVALIDATE)
    {
        map(flags);
        return reinterpret_cast<T *>(_data);
    }
    void unmap();

    void bind();
    void unbind();

    void invalidate();

    void copyData(void *data, GLsizei size);

    GLuint glName() const
    {
        return _glName;
    }

    GLenum type() const
    {
        return _type;
    }

    GLsizei size() const
    {
        return _size;
    }

    void *data() const
    {
        return _data;
    }
};

}

}

#endif /* BUFFEROBJECT_HPP_ */
