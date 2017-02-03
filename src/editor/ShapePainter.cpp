#include "ShapePainter.hpp"

#include "opengl/VertexBuffer.hpp"
#include "opengl/Shader.hpp"

#include "math/MathUtil.hpp"
#include "math/Mat4f.hpp"
#include "math/Angle.hpp"

#include "io/FileUtils.hpp"
#include "io/Path.hpp"

namespace Tungsten {

CONSTEXPR uint32 ShapePainter::MaxVertices;

using namespace GL;

VertexBuffer *ShapePainter::_vbo = nullptr;
Shader *ShapePainter::_defaultShader = nullptr;
bool ShapePainter::_initialized = false;

void ShapePainter::initialize()
{
    _vbo = new VertexBuffer(MaxVertices);
    _vbo->setStandardAttributes(VBO_ATT_POSITION | VBO_ATT_COLOR);
    _vbo->initBuffer();

    Path shaderBasePath = FileUtils::getDataPath()/"shaders/";

    _defaultShader =
        new Shader(shaderBasePath, "Preamble.txt", "Primitive.vert", "", "Primitive.frag", 1);

    _initialized = true;
}

ShapePainter::ShapePainter(DrawMode mode)
: _mode(mode),
  _color(1.0f)
{
    if (!_initialized)
        initialize();

    _defaultShader->bind();
    Vec4i viewport;
    glf->glGetIntegerv(GL_VIEWPORT, viewport.data());
    Mat4f proj(Mat4f::ortho(0.0f, float(viewport.z()), float(viewport.w()), 0.0f, -1.0f, 1.0f));
    _defaultShader->uniformMat("ModelViewProjection", proj, true);
}

ShapePainter::ShapePainter(const Mat4f &proj, DrawMode mode)
: ShapePainter(mode)
{
    _defaultShader->uniformMat("ModelViewProjection", proj, true);
}

ShapePainter::~ShapePainter()
{
    if (!_verts.empty())
        flush();
}

GLint ShapePainter::toGlMode()
{
    switch (_mode) {
    case MODE_LINES:     return GL_LINES;
    case MODE_QUADS:     return GL_TRIANGLES;
    case MODE_TRIANGLES: return GL_TRIANGLES;
    }
    return -1;
}

void ShapePainter::flush()
{
    for (uint32 i = 0; i < _verts.size(); i += MaxVertices) {
        uint32 batchSize = min(MaxVertices, uint32(_verts.size() - i));
        _vbo->bind();
        _vbo->copyData(&_verts[i], batchSize*sizeof(VboVertex));
        _vbo->draw(*_defaultShader, toGlMode(), batchSize);
    }
    _verts.clear();
}

void ShapePainter::addVertexRaw(Vec3f p)
{
    if (_mode == MODE_QUADS && (_verts.size() % 6) == 3) {
        _verts.push_back(_verts[_verts.size() - 3]);
        _verts.push_back(_verts[_verts.size() - 2]);
    }
    _verts.push_back(VboVertex{p, Vec4c(_color*255.0f)});
}

void ShapePainter::addVertexRaw(Vec2f p)
{
    addVertexRaw(Vec3f(p.x(), p.y(), 0.0f));
}

void ShapePainter::addVertex(Vec2f p)
{
    addVertexRaw(transform(p));
}

void ShapePainter::begin(DrawMode mode)
{
    if (mode != _mode)
        flush();
    _mode = mode;
}

void ShapePainter::drawRect(Vec2f pos, Vec2f size, bool filled, float width)
{
    float x0 = pos.x();
    float y0 = pos.y();
    float x1 = x0 + size.x();
    float y1 = y0 + size.y();
    if (filled) {
        begin(MODE_QUADS);
        addVertex(Vec2f(x0, y0));
        addVertex(Vec2f(x1, y0));
        addVertex(Vec2f(x1, y1));
        addVertex(Vec2f(x0, y1));
    } else {
        drawLine(Vec2f(x0, y0), Vec2f(x1, y0), width);
        drawLine(Vec2f(x1, y0), Vec2f(x1, y1), width);
        drawLine(Vec2f(x1, y1), Vec2f(x0, y1), width);
        drawLine(Vec2f(x0, y1), Vec2f(x0, y0), width);
    }
}

void ShapePainter::drawRectStipple(Vec2f pos, Vec2f size, float period, float width)
{
    float x0 = pos.x();
    float y0 = pos.y();
    float x1 = x0 + size.x();
    float y1 = y0 + size.y();
    drawLineStipple(Vec2f(x0, y0), Vec2f(x1, y0), period, width);
    drawLineStipple(Vec2f(x1, y0), Vec2f(x1, y1), period, width);
    drawLineStipple(Vec2f(x1, y1), Vec2f(x0, y1), period, width);
    drawLineStipple(Vec2f(x0, y1), Vec2f(x0, y0), period, width);
}

void ShapePainter::drawEllipseRect(Vec2f pos, Vec2f size, bool filled, float width)
{
    drawEllipse(pos + size*0.5f, size*0.5f, filled, width);
}

void ShapePainter::drawEllipse(Vec2f c, Vec2f radii, bool filled, float width)
{
    drawArc(c, radii, 0.0f, TWO_PI, filled, width);
}

void ShapePainter::drawArc(Vec2f c, Vec2f radii, float aStart, float aEnd, bool filled, float width) {
    begin(filled ? MODE_TRIANGLES : MODE_LINES);
    float r = radii.max();
    int segmentCount = max(int(TWO_PI*r/2.0f), 4); // One vertex every two pixels
    float angleStep = TWO_PI/segmentCount;

    float angle = aStart + angleStep;
    Vec2f oldP = c + Vec2f(std::cos(aStart), std::sin(aStart))*radii;
    for (int i = 0; i <= segmentCount; ++i, angle = min(aEnd, angle + angleStep)) {
        Vec2f newP = c + Vec2f(std::cos(angle), std::sin(angle))*radii;
        if (filled) {
            addVertex(c);
            addVertex(oldP);
            addVertex(newP);
        } else {
            drawLine(oldP, newP, width);
        }
        oldP = newP;
        if (angle == aEnd)
            break;
    }
}

void ShapePainter::drawLine(Vec2f x0, Vec2f x1, float width)
{
    begin(MODE_QUADS);
    x0 = transform(x0);
    x1 = transform(x1);
    Vec2f d = x1 - x0;
    Vec2f t(-d.y(), d.x());
    t.normalize();
    t *= width*0.5f;
    addVertexRaw(x0 - t);
    addVertexRaw(x0 + t);
    addVertexRaw(x1 + t);
    addVertexRaw(x1 - t);
}

void ShapePainter::drawLineStipple(Vec2f x0, Vec2f x1, float period, float width)
{
    if (period < 2.0f) {
        drawLine(x0, x1, width);
    } else {
        begin(MODE_QUADS);
        x0 = transform(x0);
        x1 = transform(x1);
        Vec2f d = x1 - x0;
        Vec2f t(-d.y(), d.x());
        t.normalize();
        t *= width*0.5f;

        period /= d.length();
        bool open = false;
        for (float f = 0.0f; f < 1.0f; f += period*0.5f, open = !open) {
            addVertexRaw(x0 + d*f - t);
            t = -t;
            addVertexRaw(x0 + d*f - t);
        }
        if (open) {
            addVertexRaw(x1 - t);
            addVertexRaw(x1 + t);
        }

        for (float f = 0.0f; f < 1.0f; f += period*0.5f)
            drawEllipse(x0 + d*f, Vec2f(width*0.5f));
    }
}

void ShapePainter::drawLine(Vec3f x0, Vec3f x1)
{
    begin(MODE_LINES);
    addVertexRaw(x0);
    addVertexRaw(x1);
}

}
