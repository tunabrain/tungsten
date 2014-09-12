#ifndef SHAPEINPUT_HPP_
#define SHAPEINPUT_HPP_

#include "AbstractPainter.hpp"

namespace Tungsten {

class ShapeInput: public AbstractPainter
{
    Vec2f _probe;

    float _minDist;
    int _closestShape;

    int _currentShape;

    void checkDistance(float d);
    Vec2f untransform(Vec2f p);

public:
    ShapeInput(Vec2f probe, float dist)
    : _probe(probe),
      _minDist(dist),
      _closestShape(-1),
      _currentShape(-1)
    {
    }

    void labelShape(int id)
    {
        _currentShape = id;
    }

    void addVertex(Vec2f x) final override;

    void begin(DrawMode /*mode*/)
    {
    }

    void drawRect(Vec2f pos, Vec2f size, bool filled = true, float width = 1.0f) final override;
    void drawRectStipple(Vec2f pos, Vec2f size, float period, float width = 1.0f) final override;
    void drawEllipseRect(Vec2f pos, Vec2f size, bool filled = true, float width = 1.0f) final override;
    void drawEllipse(Vec2f center, Vec2f radii, bool filled = true, float width = 1.0f) final override;
    void drawArc(Vec2f center, Vec2f radii, float aStart, float aEnd, bool filled = true, float width = 1.0f) final override;
    void drawLine(Vec2f x0, Vec2f x1, float width = 1.0f) final override;
    void drawLineStipple(Vec2f x0, Vec2f x1, float period, float width = 1.0f) final override;

    void setColor(const Vec3f &/*c*/) final override
    {
    }

    void setColor(const Vec4f &/*c*/) final override
    {
    }

    void setAlpha(float /*a*/) final override
    {
    }

    int closestShape()
    {
        return _closestShape;
    }
};

}

#endif /* SHAPEINPUT_HPP_ */
