#ifndef RENDER_TEXTURE_HPP
#define RENDER_TEXTURE_HPP

#include "OpenGL.hpp"

#include <vector>

namespace Tungsten {

namespace GL {

class BufferObject;

enum TexelType
{
    TEXEL_FLOAT,
    TEXEL_INT,
    TEXEL_UNSIGNED,
    TEXEL_DEPTH,
    TEXEL_DEPTH_STENCIL
};

enum TextureType
{
    TEXTURE_BUFFER,
    TEXTURE_1D,
    TEXTURE_CUBE,
    TEXTURE_2D,
    TEXTURE_3D
};

class Texture
{
    static int _maxTextureUnits;
    static int _selectedUnit;
    static int _nextTicket;
    static std::vector<int> _unitTicket;
    static std::vector<Texture *> _units;
    static size_t _memoryUsage;

    TextureType _type;
    TexelType _texelType;
    int _channels;
    int _chanBytes;

    GLuint _glName;
    GLenum _glType;
    GLenum _glFormat;
    GLenum _glChanType;
    GLenum _elementType;
    int _elementSize;

    int _width;
    int _height;
    int _depth;
    int _levels;

    int _boundUnit;

    static void initTextureUnits();

    void selectUnit(int unit);
    void markAsUsed(int unit);
    int selectVictimUnit();

public:
    Texture(TextureType type, int width, int height = 1, int depth = 1, int levels = 1);
    ~Texture();

    Texture(Texture &&o);
    Texture &operator=(Texture &&o);

    Texture(const Texture &) = delete;
    Texture &operator=(const Texture &) = delete;

    void setFormat(TexelType texel, int channels, int chanBytes);
    void setFilter(bool clamp, bool linear);
    void init(GLuint bufferObject = -1);

    void copy(void *data, int level = 0);
    void copyPbo(BufferObject& pbo, int level = 0);

    void bind(int unit);
    void bindAny();

    size_t size();

    TextureType type() const
    {
        return _type;
    }

    TexelType texelType() const
    {
        return _texelType;
    }

    int channels() const
    {
        return _channels;
    }

    int bpChannel() const
    {
        return _chanBytes;
    }

    GLuint glName() const
    {
        return _glName;
    }

    GLenum glType() const
    {
        return _glType;
    }

    int width() const
    {
        return _width;
    }

    int height() const
    {
        return _height;
    }

    int depth() const
    {
        return _depth;
    }

    int levels() const
    {
        return _levels;
    }

    int boundUnit()
    {
        return _boundUnit;
    }

    static size_t memoryUsage()
    {
        return _memoryUsage;
    }
};

}

}

#endif /* RENDER_TEXTURE_HPP_ */
