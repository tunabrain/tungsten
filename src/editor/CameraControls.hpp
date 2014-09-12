#ifndef CAMERACONTROLS_HPP_
#define CAMERACONTROLS_HPP_

#include "math/MathUtil.hpp"
#include "math/Angle.hpp"
#include "math/Mat4f.hpp"
#include "math/Vec.hpp"

#include <QMouseEvent>
#include <cmath>

#include <iostream>

namespace Tungsten {

class CameraControls
{
    bool _isPressed;
    int _oldX;
    int _oldY;

    Vec3f _center;
    Vec3f _coords;

    void tumble(float x, float y)
    {
        _coords.x() += 0.5f*x*PI;
        _coords.y() += 0.5f*y*PI;

        _coords.x() = std::fmod(_coords.x(), TWO_PI);
        _coords.y() = clamp(_coords.y(), -PI*0.4999f, PI*0.4999f);
    }

    void pan(float x, float y)
    {
        _center += toMatrix().transformVector(Vec3f(-x, y, 0.0f))*_coords.z()*0.5f;
    }

    void zoom(float x, float /*y*/)
    {
        _coords.z() *= 1.0f - x;
        _coords.z() = clamp(_coords.z(), 1e-3f, 1e3f);
    }

public:
    Vec3f localPos() const
    {
        return Vec3f(
             std::cos(_coords.y())*std::sin(_coords.x()),
             std::sin(_coords.y()),
            -std::cos(_coords.y())*std::cos(_coords.x())
        )*_coords.z();
    }

    Vec3f lookAt() const
    {
        return _center;
    }

    Vec3f globalPos() const
    {
        return localPos() + _center;
    }

    Vec3f up() const
    {
        return Vec3f(0.0f, 1.0f, 0.0f);
    }

    Mat4f toMatrix()
    {
        return Mat4f::lookAt(globalPos(), lookAt() - globalPos(), up());
    }

    bool update(const QMouseEvent *event, int /*w*/, int h, float fov)
    {
        bool    altDown = event->modifiers() & Qt::AltModifier;
        bool   ctrlDown = event->modifiers() & Qt::ControlModifier;
        bool   leftDown = event->buttons() & Qt::LeftButton;
        bool  rightDown = event->buttons() & Qt::RightButton;
        bool middleDown = event->buttons() & Qt::MiddleButton;
        bool anyDown    = leftDown || rightDown || middleDown;

        int x = event->x();
        int y = event->y();

        bool captureInput = false;
        if (anyDown) {
            float factor = 1.0f/(h*std::tan(fov*0.5f));
            float moveX = (x - _oldX)*2.0f*factor;
            float moveY = (y - _oldY)*2.0f*factor;

            bool isPan =
                (altDown && ((leftDown && rightDown) || middleDown)) ||
                (ctrlDown && leftDown);
            bool isTumble = altDown && leftDown;
            bool isZoom = altDown && rightDown;

            captureInput = isPan || isTumble || isZoom;

            if (_isPressed) {
                if (isPan)
                    pan(moveX, moveY);
                else if (isTumble)
                    tumble(moveX, moveY);
                else if (isZoom)
                    zoom(moveX, moveY);
            }
            _oldX = x;
            _oldY = y;
            _isPressed = true;
        } else {
            _isPressed = false;
        }
        return captureInput;
    }

    void set(const Vec3f &pos, const Vec3f &lookAt, const Vec3f &/*up*/)
    {
        Vec3f dir = pos - lookAt;

        _center = lookAt;
        _coords = Vec3f(std::atan2(dir.x(), -dir.z()), std::asin(dir.y()/dir.length()), dir.length());
    }
};

}


#endif /* CAMERACONTROLS_HPP_ */
