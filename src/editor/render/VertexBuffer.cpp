#include <GL/glew.h>

#include "VertexBuffer.hpp"
#include "Shader.hpp"

#include "extern/tinyformat/tinyformat.hpp"

#include "GLDebug.hpp"

namespace Tungsten {

namespace GL {

static int glTypeSize(GLenum type) {
    switch (type) {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:
        return 1;
    case GL_SHORT:
    case GL_UNSIGNED_SHORT:
        return 2;
    case GL_INT:
    case GL_UNSIGNED_INT:
    case GL_FLOAT:
        return 4;
    case GL_DOUBLE:
        return 8;
    default:
        return 0;
    }
}

VertexBuffer::VertexBuffer(int length) : BufferObject(ARRAY_BUFFER),
        _length(length), _elementSize(0), _attributeCount(0) {}

void VertexBuffer::initBuffer() {
    init(_length*_elementSize);
}

void VertexBuffer::addAttribute(const char *name, GLint size, GLenum type, int norm) {
    _attributes[_attributeCount++] = VertexAttrib(name, size, norm, type, _elementSize);
    _elementSize += size*glTypeSize(type);
}

void VertexBuffer::setStandardAttributes(int attributes) {
    for (int i = 0; i < VBO_ATT_COUNT; i++) {
        if (attributes & (1 << i)) {
            switch (1 << i) {
            case VBO_ATT_POSITION:
                addAttribute("Position", 3, GL_FLOAT, 0);
                break;
            case VBO_ATT_NORMAL:
                addAttribute("Normal", 3, GL_FLOAT, 0);
                break;
            case VBO_ATT_COLOR:
                addAttribute("Color", 4, GL_UNSIGNED_BYTE, 1);
                break;
            case VBO_ATT_TEXCOORD0:
                addAttribute("TexCoord0", 2, GL_FLOAT, 0);
                break;
            case VBO_ATT_TEXCOORD1:
                addAttribute("TexCoord1", 2, GL_FLOAT, 0);
                break;
            }
        }
    }
}

void VertexBuffer::enableVertexAttrib(int i) {
    const VertexAttrib& a = _attributes[i];

    if (a.index >= 0) {
        glEnableVertexAttribArray(a.index);
        glVertexAttribPointer(a.index, a.size, a.type, a.norm, _elementSize,
                (const GLvoid *) a.offset);
    }
}

void VertexBuffer::disableVertexAttrib(int i) {
    if (_attributes[i].index >= 0) {
        glDisableVertexAttribArray(_attributes[i].index);
    }
}

void VertexBuffer::enableVertexAttributes() {
    for (int i = 0; i < _attributeCount; i++)
        enableVertexAttrib(i);
}

void VertexBuffer::disableVertexAttributes() {
    for (int i = 0; i < _attributeCount; i++)
        disableVertexAttrib(i);
}

void VertexBuffer::mapAttributes(Shader &shader) {
    for (int i = 0; i < _attributeCount; i++) {
        _attributes[i].index = glGetAttribLocation(shader.program(), _attributes[i].name);
    }
}

void VertexBuffer::draw(Shader &shader, GLenum mode, int count) {
    GLCHECK();
    bind();
    GLCHECK();
    mapAttributes(shader);
    GLCHECK();
    enableVertexAttributes();
    GLCHECK();
    glDrawArrays(mode, 0, count ? count : _length);
    GLCHECK();
    disableVertexAttributes();
    GLCHECK();
    unbind();
    GLCHECK();
}

void VertexBuffer::drawIndexed(BufferObject &ibo, Shader &shader, GLenum mode, int count) {
    bind();
    ibo.bind();
    mapAttributes(shader);
    enableVertexAttributes();
    glDrawElements(mode, count ? count : ibo.size()/sizeof(uint32), GL_UNSIGNED_INT, NULL);
    disableVertexAttributes();
    ibo.unbind();
    unbind();
}

}

}
