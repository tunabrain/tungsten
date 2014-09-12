#ifndef ABSTRACTPAINTER_HPP_
#define ABSTRACTPAINTER_HPP_

#include "math/Vec.hpp"

namespace Tungsten {

class AbstractPainter
{
protected:
    Vec2f _x, _y, _base;

    Vec2f transform(Vec2f p)
    {
        return _x*p.x() + _y*p.y() + _base;
    }

public:
    enum DrawMode
    {
        MODE_LINES,
        MODE_QUADS,
        MODE_TRIANGLES,
    };

    AbstractPainter()
    : _x(1.0f, 0.0f),
      _y(0.0f, 1.0f),
      _base(0.0f)
    {
    }

    virtual ~AbstractPainter()
    {
    }

    virtual void labelShape(int id) = 0;

    virtual void addVertex(Vec2f x) = 0;
    virtual void begin(DrawMode mode) = 0;

    virtual void drawRect(Vec2f pos, Vec2f size, bool filled = true, float width = 1.0f) = 0;
    virtual void drawRectStipple(Vec2f pos, Vec2f size, float period, float width = 1.0f) = 0;
    virtual void drawEllipseRect(Vec2f pos, Vec2f size, bool filled = true, float width = 1.0f) = 0;
    virtual void drawEllipse(Vec2f center, Vec2f radii, bool filled = true, float width = 1.0f) = 0;
    virtual void drawArc(Vec2f center, Vec2f radii, float aStart, float aEnd, bool filled = true, float width = 1.0f) = 0;
    virtual void drawLine(Vec2f x0, Vec2f x1, float width = 1.0f) = 0;
    virtual void drawLineStipple(Vec2f x0, Vec2f x1, float period, float width = 1.0f) = 0;

    virtual void setColor(const Vec3f &c) = 0;
    virtual void setColor(const Vec4f &c) = 0;
    virtual void setAlpha(float a) = 0;

    void setTransform(Vec2f x, Vec2f y, Vec2f pos)
    {
        _x = x;
        _y = y;
        _base = pos;
    }
};

}

#endif /* ABSTRACTPAINTER_HPP_ */
