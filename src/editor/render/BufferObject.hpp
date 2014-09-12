#ifndef RENDER_BUFFEROBJECT_HPP_
#define RENDER_BUFFEROBJECT_HPP_

#include <GL/glew.h>

namespace Tungsten {

namespace GL {

enum BufferType
{
    ARRAY_BUFFER,
    ELEMENT_ARRAY_BUFFER,
    PIXEL_PACK_BUFFER,
    PIXEL_UNPACK_BUFFER,
    SHADER_STORAGE_BUFFER,
    UNIFORM_BUFFER
};

enum MapFlags
{
    MAP_READ             = (1 << 0),
    MAP_WRITE            = (1 << 1),
    MAP_INVALIDATE_RANGE = (1 << 2),
    MAP_INVALIDATE       = (1 << 3),
    MAP_FLUSH_EXPLICIT   = (1 << 4),
    MAP_UNSYCHRONIZED    = (1 << 5)
};

class BufferObject {
public:
    BufferType _type;
    GLenum _glType;
    GLuint _glName;
    GLsizei _size;

    void *_data;

public:
    virtual ~BufferObject();

    BufferObject(BufferType type);
    BufferObject(BufferType type, GLsizei size);
    void init(GLsizei size);

    void map(int flags = MAP_READ | MAP_WRITE | MAP_INVALIDATE);

    template<typename T>
    T *map(int flags = MAP_READ | MAP_WRITE | MAP_INVALIDATE)
    {
        map(flags);
        return reinterpret_cast<T *>(_data);
    }

    void mapRange(GLintptr offset, GLsizeiptr length, int flags = MAP_READ | MAP_WRITE | MAP_INVALIDATE_RANGE);
    void unmap();

    void bind();
    void unbind();

    void invalidate();
    void invalidateRange(GLintptr offset, GLsizeiptr length);

    void bindIndexed(int index);
    void bindIndexedRange(int index, GLintptr offset, GLsizeiptr size);
    void unbindIndexed(int index);

    void copyData(void *data, GLsizei size, GLenum usage);

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
