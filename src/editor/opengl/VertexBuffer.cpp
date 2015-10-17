#include "OpenGL.hpp"

#include "VertexBuffer.hpp"
#include "GlUtils.hpp"
#include "Shader.hpp"

#include <tinyformat/tinyformat.hpp>

namespace Tungsten {

namespace GL {

VertexBuffer::VertexBuffer(int length)
: BufferObject(ARRAY_BUFFER),
  _length(length),
  _elementSize(0)
{
}

VertexBuffer::VertexBuffer(VertexBuffer &&o)
: BufferObject(std::move(o))
{
    _length      = o._length;
    _elementSize = o._elementSize;
    _attributes  = std::move(o._attributes);
}

VertexBuffer &VertexBuffer::operator=(VertexBuffer &&o)
{
    BufferObject::operator=(std::move(o));

    _length      = o._length;
    _elementSize = o._elementSize;
    _attributes  = std::move(o._attributes);

    return *this;
}

void VertexBuffer::initBuffer()
{
    init(_length*_elementSize);
}

void VertexBuffer::addAttribute(std::string name, GLint size, GLenum type, bool norm)
{
    _attributes.emplace_back(VertexAttrib{
        std::move(name),
        size,
        norm,
        type,
        _elementSize,
        -1
    });
    _elementSize += size*glTypeSize(type);
}

void VertexBuffer::setStandardAttributes(int attributes)
{
    for (int i = 0; i < VBO_ATT_COUNT; i++) {
        if (attributes & (1 << i)) {
            switch (1 << i) {
            case VBO_ATT_POSITION:
                addAttribute("Position", 3, GL_FLOAT, false);
                break;
            case VBO_ATT_NORMAL:
                addAttribute("Normal", 3, GL_FLOAT, false);
                break;
            case VBO_ATT_COLOR:
                addAttribute("Color", 4, GL_UNSIGNED_BYTE, true);
                break;
            case VBO_ATT_TEXCOORD0:
                addAttribute("TexCoord0", 2, GL_FLOAT, false);
                break;
            case VBO_ATT_TEXCOORD1:
                addAttribute("TexCoord1", 2, GL_FLOAT, false);
                break;
            }
        }
    }
}

void VertexBuffer::enableVertexAttrib(int i)
{
    const VertexAttrib& a = _attributes[i];

    if (a.index >= 0) {
        glf->glEnableVertexAttribArray(a.index);
        glf->glVertexAttribPointer(a.index, a.size, a.type, a.norm, _elementSize,
                reinterpret_cast<const GLvoid *>(a.offset));
    }
}

void VertexBuffer::disableVertexAttrib(int i)
{
    if (_attributes[i].index >= 0)
        glf->glDisableVertexAttribArray(_attributes[i].index);
}

void VertexBuffer::enableVertexAttributes()
{
    for (size_t i = 0; i < _attributes.size(); i++)
        enableVertexAttrib(i);
}

void VertexBuffer::disableVertexAttributes()
{
    for (size_t i = 0; i < _attributes.size(); i++)
        disableVertexAttrib(i);
}

void VertexBuffer::mapAttributes(Shader &shader)
{
    for (size_t i = 0; i < _attributes.size(); i++)
        _attributes[i].index = glf->glGetAttribLocation(shader.program(), _attributes[i].name.c_str());
}

void VertexBuffer::draw(Shader &shader, GLenum mode, int count)
{
    bind();
    mapAttributes(shader);
    enableVertexAttributes();
    glf->glDrawArrays(mode, 0, count ? count : _length);
    disableVertexAttributes();
    unbind();
}

void VertexBuffer::drawIndexed(BufferObject &ibo, Shader &shader, GLenum mode, int count)
{
    bind();
    ibo.bind();
    mapAttributes(shader);
    enableVertexAttributes();
    glf->glDrawElements(mode, count ? count : ibo.size()/sizeof(uint32), GL_UNSIGNED_INT, nullptr);
    disableVertexAttributes();
    ibo.unbind();
    unbind();
}

}

}
