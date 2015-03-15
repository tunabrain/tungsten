#ifndef RENDER_VBOBUFFER_HPP_
#define RENDER_VBOBUFFER_HPP_

#include "BufferObject.hpp"

#include <string>
#include <vector>

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
        std::string name;
        int size;
        bool norm;
        GLenum type;
        size_t offset;
        GLint index;
    };

    int _length;
    size_t _elementSize;

    std::vector<VertexAttrib> _attributes;

    void enableVertexAttrib(int index);
    void disableVertexAttrib(int index);

public:
    VertexBuffer(int length);

    VertexBuffer(VertexBuffer &&o);
    VertexBuffer &operator=(VertexBuffer &&o);

    VertexBuffer(const VertexBuffer &) = delete;
    VertexBuffer &operator=(const VertexBuffer &) = delete;

    void addAttribute(std::string name, GLint size, GLenum type, bool norm);
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
        return _attributes.size();
    }
};

}

}

#endif /* RENDER_VBOBUFFER_HPP_ */
