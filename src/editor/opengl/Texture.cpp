#include "OpenGL.hpp"

#include "BufferObject.hpp"
#include "Texture.hpp"
#include "Debug.hpp"

#include <algorithm>

namespace Tungsten {

namespace GL {

static const GLenum GlFormatTable[][4][4] = {
    {{  GL_R8,   GL_RG8,   GL_RGB8,   GL_RGBA8},
     {GL_R16F, GL_RG16F, GL_RGB16F, GL_RGBA16F},
     {      0,        0,         0,          0},
     {GL_R32F, GL_RG32F, GL_RGB32F, GL_RGBA32F}},

    {{ GL_R8I,  GL_RG8I,  GL_RGB8I,  GL_RGBA8I},
     {GL_R16I, GL_RG16I, GL_RGB16I, GL_RGBA16I},
     {      0,        0,         0,          0},
     {GL_R32I, GL_RG32I, GL_RGB32I, GL_RGBA32I}},

    {{ GL_R8UI,  GL_RG8UI,  GL_RGB8UI,  GL_RGBA8UI},
     {GL_R16UI, GL_RG16UI, GL_RGB16UI, GL_RGBA16UI},
     {       0,         0,          0,           0},
     {GL_R32UI, GL_RG32UI, GL_RGB32UI, GL_RGBA32UI}},

    {{                   0, 0, 0, 0},
     {GL_DEPTH_COMPONENT16, 0, 0, 0},
     {GL_DEPTH_COMPONENT24, 0, 0, 0},
     {GL_DEPTH_COMPONENT32, 0, 0, 0}},

    {{                   0, 0, 0, 0},
     {                   0, 0, 0, 0},
     { GL_DEPTH24_STENCIL8, 0, 0, 0},
     {GL_DEPTH32F_STENCIL8, 0, 0, 0}},
};

static const GLenum GlTypeTable[][4] = {
        {GL_UNSIGNED_BYTE,          GL_FLOAT,        0,        GL_FLOAT},
        {         GL_BYTE,          GL_SHORT,        0,          GL_INT},
        {GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT,        0, GL_UNSIGNED_INT},
        {               0,          GL_FLOAT, GL_FLOAT,        GL_FLOAT},
        {               0,                 0, GL_FLOAT,        GL_FLOAT},
};

static const GLenum GlTexTable[] = {
        GL_TEXTURE_BUFFER,
        GL_TEXTURE_1D,
        GL_TEXTURE_CUBE_MAP,
        GL_TEXTURE_2D,
        GL_TEXTURE_3D,
};

static const GLenum GlChanTable[][4] = {
        {GL_RED, GL_RG, GL_RGB, GL_RGBA},
        {GL_RED_INTEGER, GL_RG_INTEGER, GL_RGB_INTEGER, GL_RGBA_INTEGER},
        {GL_RED_INTEGER, GL_RG_INTEGER, GL_RGB_INTEGER, GL_RGBA_INTEGER},
        {GL_DEPTH_COMPONENT, 0, 0, 0},
        {GL_DEPTH_STENCIL, 0, 0, 0},
};

int Texture::_maxTextureUnits = 0;
int Texture::_selectedUnit = 0;
int Texture::_nextTicket = 1;
std::vector<int> Texture::_unitTicket;
std::vector<Texture *> Texture::_units;
size_t Texture::_memoryUsage = 0;

void Texture::initTextureUnits()
{
    glf->glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &_maxTextureUnits);
    _unitTicket.resize(_maxTextureUnits, 0);
    _units.resize(_maxTextureUnits, nullptr);
}

Texture::Texture(TextureType type, int width, int height, int depth, int levels)
: _type(type),
  _texelType(TEXEL_FLOAT),
  _channels(0),
  _chanBytes(0),
  _glName(0),
  _glFormat(0),
  _glChanType(0),
  _elementType(0),
  _elementSize(0),
  _levels(levels),
  _boundUnit(-1)
{
    if (_maxTextureUnits == 0)
        initTextureUnits();

    _width = _height = _depth = 1;
    _glType = GlTexTable[_type];

    if (_type > TEXTURE_BUFFER)
        _width = width;
    if (_type > TEXTURE_1D)
        _height = height;
    if (_type > TEXTURE_2D)
        _depth = depth;
}

Texture::~Texture()
{
    if (_glName) {
        _memoryUsage -= size();
        glf->glDeleteTextures(1, &_glName);
    }
}

Texture::Texture(Texture &&o)
{
    _type        = o._type;
    _texelType   = o._texelType;
    _channels    = o._channels;
    _chanBytes   = o._chanBytes;

    _glName      = o._glName;
    _glType      = o._glType;
    _glFormat    = o._glFormat;
    _glChanType  = o._glChanType;
    _elementType = o._elementType;
    _elementSize = o._elementSize;

    _width       = o._width;
    _height      = o._height;
    _depth       = o._depth;
    _levels      = o._levels;

    _boundUnit   = o._boundUnit;

    o._glName = 0;
}

Texture &Texture::operator=(Texture &&o)
{
    _type        = o._type;
    _texelType   = o._texelType;
    _channels    = o._channels;
    _chanBytes   = o._chanBytes;

    _glName      = o._glName;
    _glType      = o._glType;
    _glFormat    = o._glFormat;
    _glChanType  = o._glChanType;
    _elementType = o._elementType;
    _elementSize = o._elementSize;

    _width       = o._width;
    _height      = o._height;
    _depth       = o._depth;
    _levels      = o._levels;

    _boundUnit   = o._boundUnit;

    o._glName = 0;

    return *this;
}

void Texture::selectUnit(int unit)
{
    if (unit != _selectedUnit) {
        glf->glActiveTexture(GL_TEXTURE0 + unit);
        _selectedUnit = unit;
    }
}

void Texture::markAsUsed(int unit)
{
    _unitTicket[unit] = _nextTicket++;
}

int Texture::selectVictimUnit()
{
    int leastTicket = _unitTicket[0];
    int leastUnit = 0;

    for (int i = 1; i < _maxTextureUnits; i++) {
        if (_unitTicket[i] < leastTicket) {
            leastTicket = _unitTicket[i];
            leastUnit = i;
        }
    }

    return leastUnit;
}

void Texture::setFormat(TexelType texel, int channels, int chan_bytes)
{
    _texelType = texel;
    _channels = channels;
    _chanBytes = chan_bytes;

    ASSERT(channels == 1 || channels == 2 || channels == 4,
            "Number of channels must be 1, 2 or 4\n");

    _glFormat = GlFormatTable[texel][chan_bytes - 1][channels - 1];
    _glChanType = GlChanTable[texel][channels - 1];
    _elementType = GlTypeTable[texel][chan_bytes - 1];
    _elementSize = chan_bytes * channels;

    ASSERT(_glFormat != 0 && _elementType != 0, "Invalid format combination\n");
}

void Texture::setFilter(bool clamp, bool linear)
{
    GLenum coordMode = (clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    GLenum interpMode = (linear ? GL_LINEAR : GL_NEAREST);

    bindAny();

    if (_type > TEXTURE_BUFFER)
        glf->glTexParameteri(_glType, GL_TEXTURE_WRAP_S, coordMode);
    if (_type > TEXTURE_1D)
        glf->glTexParameteri(_glType, GL_TEXTURE_WRAP_T, coordMode);
    if (_type > TEXTURE_2D || _type == TEXTURE_CUBE)
        glf->glTexParameteri(_glType, GL_TEXTURE_WRAP_R, coordMode);

    if (_type != TEXTURE_BUFFER) {
        glf->glTexParameteri(_glType, GL_TEXTURE_MIN_FILTER, interpMode);
        glf->glTexParameteri(_glType, GL_TEXTURE_MAG_FILTER, interpMode);
        glf->glTexParameteri(_glType, GL_TEXTURE_MAX_LEVEL, _levels - 1);
    }
}

void Texture::init(GLuint bufferObject)
{
    glf->glGenTextures(1, &_glName);

    bindAny();

    switch (_type) {
    case TEXTURE_BUFFER:
        glf->glTexBuffer(GL_TEXTURE_BUFFER, _glFormat, bufferObject);
        break;
    case TEXTURE_1D:
        glf->glTexImage1D(GL_TEXTURE_1D, 0, _glFormat, _width, 0, _glChanType, _elementType, nullptr);
        break;
    case TEXTURE_CUBE:
        glf->glTexImage2D(GL_TEXTURE_CUBE_MAP, 0, _glFormat, _width, _height, 0, _glChanType, _elementType, nullptr);
        break;
    case TEXTURE_2D:
        glf->glTexImage2D(GL_TEXTURE_2D, 0, _glFormat, _width, _height, 0, _glChanType, _elementType, nullptr);
        break;
    case TEXTURE_3D:
        glf->glTexImage3D(GL_TEXTURE_3D, 0, _glFormat, _width, _height, _depth, 0, _glChanType, _elementType, nullptr);
        break;
    }

    _memoryUsage += size();

    setFilter(true, true);
}

void Texture::copy(void *data, int level)
{
    bindAny();

    int w = std::max(_width  >> level, 1);
    int h = std::max(_height >> level, 1);
    int d = std::max(_depth  >> level, 1);

    switch (_type) {
    case TEXTURE_BUFFER:
        FAIL("Texture copy not available for texture buffer - use BufferObject::copyData instead");
        break;
    case TEXTURE_1D:
        glf->glTexSubImage1D(GL_TEXTURE_1D, level, 0, w, _glChanType, _elementType, data);
        break;
    case TEXTURE_CUBE:
        for (int i = 0; i < 6; i++) {
            glf->glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, level, 0, 0,
                                 w, h, _glChanType, _elementType, data);

            if (data)
                data = (char *)data + w*h*_elementSize;
        }
        break;
    case TEXTURE_2D:
        glf->glTexSubImage2D(GL_TEXTURE_2D, level, 0, 0, w, h,
                             _glChanType, _elementType, data);
        break;
    case TEXTURE_3D:
        glf->glTexSubImage3D(GL_TEXTURE_3D, level, 0, 0, 0, w, h, d,
                             _glChanType, _elementType, data);
        break;
    }
}

void Texture::copyPbo(BufferObject &pbo, int level)
{
    pbo.bind();

    switch (_type) {
    case TEXTURE_BUFFER:
        FAIL("PBO copy not available for texture buffer - use BufferObject::copyData instead");
        break;
    default:
        copy(nullptr, level);
    }

    pbo.unbind();
}

void Texture::bind(int unit)
{
    if (_units[unit])
        _units[unit]->_boundUnit = -1;

    markAsUsed(unit);
    selectUnit(unit);

    if (_boundUnit == unit)
        return;

    _units[unit] = this;
    glf->glBindTexture(_glType, _glName);
    _boundUnit = unit;
}

void Texture::bindAny()
{
    if (_boundUnit != -1) {
        markAsUsed(_boundUnit);
        selectUnit(_boundUnit);
        return;
    } else
        bind(selectVictimUnit());
}

size_t Texture::size()
{
    switch (_type) {
    case TEXTURE_BUFFER:
    case TEXTURE_1D:
        return size_t(_width)*_elementSize;
    case TEXTURE_CUBE:
        return ((size_t(_width)*_height)*_elementSize)*6;
    case TEXTURE_2D:
        return (size_t(_width)*_height)*_elementSize;
    case TEXTURE_3D:
        return ((size_t(_width)*_height)*_depth)*_elementSize;
    }
    return 0;
}

}

}
