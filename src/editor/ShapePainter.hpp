#ifndef SHAPEPAINTER_HPP_
#define SHAPEPAINTER_HPP_

#include "AbstractPainter.hpp"

#include "opengl/OpenGL.hpp"

#include <vector>

namespace Tungsten {

class Mat4f;

namespace GL {
class VertexBuffer;
class Shader;
}

class ShapePainter: public AbstractPainter
{
    static CONSTEXPR uint32 MaxVertices = 1 << 16;
    static GL::VertexBuffer *_vbo;
    static GL::Shader *_defaultShader;
    static bool _initialized;
    static void initialize();

    struct VboVertex
    {
        Vec3f pos;
        Vec4c color;
    };

    DrawMode _mode;
    Vec4f _color;
    std::vector<VboVertex> _verts;

    GLint toGlMode();
    void flush();

    void addVertexRaw(Vec3f x);
    void addVertexRaw(Vec2f x);

public:
    ShapePainter(DrawMode mode = MODE_LINES);
    ShapePainter(const Mat4f &proj, DrawMode mode = MODE_LINES);
    ~ShapePainter();

    void labelShape(int /*id*/) final override
    {
    }

    void addVertex(Vec2f x) final override;
    void begin(DrawMode mode) final override;

    void drawRect(Vec2f pos, Vec2f size, bool filled = true, float width = 1.0f) final override;
    void drawRectStipple(Vec2f pos, Vec2f size, float period, float width = 1.0f) final override;
    void drawEllipseRect(Vec2f pos, Vec2f size, bool filled = true, float width = 1.0f) final override;
    void drawEllipse(Vec2f center, Vec2f radii, bool filled = true, float width = 1.0f) final override;
    void drawArc(Vec2f center, Vec2f radii, float aStart, float aEnd, bool filled = true, float width = 1.0f) final override;
    void drawLine(Vec2f x0, Vec2f x1, float width = 1.0f) final override;
    void drawLineStipple(Vec2f x0, Vec2f x1, float period, float width = 1.0f) final override;

    void drawLine(Vec3f x0, Vec3f x1);

    void setColor(const Vec3f &c) final override
    {
        _color = Vec4f(c.x(), c.y(), c.z(), 1.0f);
    }

    void setColor(const Vec4f &c) final override
    {
        _color = c;
    }

    void setAlpha(float a) final override
    {
        _color.w() = a;
    }
};

}

#endif /* SHAPEPAINTER_HPP_ */
