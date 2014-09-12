#include "ShapeInput.hpp"

#include "math/MathUtil.hpp"
#include "math/Angle.hpp"

#include <iostream>

namespace Tungsten {

void ShapeInput::checkDistance(float d)
{
    if (_currentShape != -1 && d < _minDist) {
        _minDist = d;
        _closestShape = _currentShape;
    }
}

Vec2f ShapeInput::untransform(Vec2f p)
{
    p -= _base;
    return (Vec2f(_y.y(), -_x.y())*p.x() + Vec2f(-_y.x(), _x.x())*p.y())/(_x.x()*_y.y() - _x.y()*_y.x());
}

void ShapeInput::drawRect(Vec2f pos, Vec2f size, bool /*filled*/, float width)
{
    pos += size*0.5f;
    checkDistance(max(std::abs(_probe - pos) - size*0.5f, Vec2f(0.0f)).length() - width);
}

void ShapeInput::drawRectStipple(Vec2f pos, Vec2f size, float /*period*/, float width)
{
    return drawRect(pos, size, false, width);
}

void ShapeInput::addVertex(Vec2f x)
{
    checkDistance((transform(x) - _probe).length());
}

void ShapeInput::drawEllipseRect(Vec2f pos, Vec2f size, bool filled, float width)
{
    drawEllipse(pos + size*0.5f, size*0.5f, filled, width);
}

void ShapeInput::drawEllipse(Vec2f c, Vec2f radii, bool filled, float width)
{
    drawArc(c, radii, 0.0f, TWO_PI, filled, width);
}

void ShapeInput::drawArc(Vec2f c, Vec2f radii, float aStart, float aEnd, bool /*filled*/, float width)
{
    for (float angle = aStart; angle < aEnd; angle += Angle::degToRad(0.5f))
        checkDistance((transform(c + Vec2f(std::cos(angle), std::sin(angle))*radii) - _probe).length() - width);
}

void ShapeInput::drawLine(Vec2f x0, Vec2f x1, float width)
{
    x0 = transform(x0);
    x1 = transform(x1);
    Vec2f l = x1 - x0;
    width *= 0.5f;
    float t = (_probe - x0).dot(l)/l.lengthSq();
    if (t < 0.0f)
        checkDistance((_probe - x0).length() - width);
    else if (t > 1.0f)
        checkDistance((_probe - x1).length() - width);
    else
        checkDistance(std::abs((_probe - x0).dot(Vec2f(-l.y(), l.x())))/l.length() - width);
}

void ShapeInput::drawLineStipple(Vec2f x0, Vec2f x1, float /*period*/, float width)
{
    drawLine(x0, x1, width);
}

}
