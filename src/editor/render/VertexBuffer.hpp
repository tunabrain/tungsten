#ifndef RENDER_VBOBUFFER_HPP_
#define RENDER_VBOBUFFER_HPP_

#include "BufferObject.hpp"

#define MAX_VBO_ATTRIBUTES 16

namespace Tungsten {

namespace GL {

enum VboAttributes
{
    VBO_ATT_POSITION  = (1 << 0),
    VBO_ATT_NORMAL    = (1 << 1),
    VBO_ATT_COLOR     = (1 << 2),
    VBO_ATT_TEXCOORD0 = (1 << 3),
    VBO_ATT_TEXCOORD1 = (1 << 4),
    VBO_ATT_COUNT     = 5
};

class Shader;

class VertexBuffer: public BufferObject
{
    struct VertexAttrib
    {
        const char *name;
        int size;
        bool norm;
        GLenum type;
        size_t offset;
        GLint index;

        VertexAttrib() = default;
        VertexAttrib(const char *_name, int _size, bool _norm, GLenum _type, size_t _offset)
        : name(_name), size(_size), norm(_norm), type(_type), offset(_offset), index(-1)
        {}
    };

    int _length;
    int _elementSize;

    int _attributeCount;
    VertexAttrib _attributes[MAX_VBO_ATTRIBUTES];

    void enableVertexAttrib(int index);
    void disableVertexAttrib(int index);

public:
    VertexBuffer(int length);

    void addAttribute(const char *name, GLint size, GLenum type, int norm);
    void setStandardAttributes(int attributes);

    void initBuffer();

    void enableVertexAttributes();
    void disableVertexAttributes();

    void mapAttributes(Shader &shader);
    void draw(Shader &shader, GLenum mode, int count = 0);
    void drawIndexed(BufferObject &ibo, Shader &shader, GLenum mode, int count = 0);

    int length() const
    {
        return _length;
    }

    int elementSize() const
    {
        return _elementSize;
    }

    int attributeCount() const
    {
        return _attributeCount;
    }
};

}

}

#endif /* RENDER_VBOBUFFER_HPP_ */
