#include "TransformGizmo.hpp"

#include "ShapePainter.hpp"
#include "ShapeInput.hpp"

#include "math/Angle.hpp"
#include "math/MathUtil.hpp"
#include <iostream>

#include <QMouseEvent>

namespace Tungsten {

TransformGizmo::TransformGizmo()
: _mode(MODE_TRANSLATE),
  _needsMouseDown(false),
  _translateLocal(false),
  _snapToGrid(false),
  _transforming(false),
  _hoverShape(-1)
{
}

Vec2f TransformGizmo::project(const Vec3f &p) const
{
    Vec4f z = _projection*(_invView*Vec4f(p.x(), p.y(), p.z(), 1.0f));
    z /= z.w();
    z.x() = (z.x()*0.5f + 0.5f)*_w;
    z.y() = (0.5f - z.y()*0.5f)*_h;

    return Vec2f(z.x(), z.y());
}

Vec3f TransformGizmo::viewVector(const Vec2f& p) const
{
    return _view.transformVector(Vec3f(
        (p.x()*2.0f/_w - 1.0f)/_projection[0],
        (1.0f - p.y()*2.0f/_h)/_projection[5],
        1.0f
    )).normalized();
}

Vec3f TransformGizmo::unproject(const Vec2f &p, float depth) const
{
    return _view*Vec3f(
        (p.x()*2.0f/_w - 1.0f)*depth/_projection[0],
        (1.0f - p.y()*2.0f/_h)*depth/_projection[5],
        depth
    );
}

Vec3f TransformGizmo::intersectPlane(const Vec2f &p, const Vec3f &base, const Vec3f &n) const
{
    Vec3f pos = _view*Vec3f(0.0f);
    Vec3f dir = viewVector(p);

    return pos + (base - pos).dot(n)/dir.dot(n)*dir;
}

Vec2f TransformGizmo::projectAxis(const Mat4f &mat, int axis) const
{
    Vec3f p(0.0f);
    Vec2f a = project(mat*p);
    p[axis] = 1.0f;
    Vec2f b = project(mat*p);
    return b - a;
}

float TransformGizmo::relativeMovement(int dim, const Vec2f &move) const
{
    Vec2f axis = projectAxis(_fixedTransform, dim);
    return move.dot(axis)/axis.lengthSq();
}

void TransformGizmo::computeTranslation()
{
    Vec3f begin, current;
    if (_fixedAxis == -1) {
        float depth   = (_invView*(_fixedTransform*Vec3f(0.0f))).z();
        begin   = unproject(Vec2f(  _beginX,   _beginY), depth);
        current = unproject(Vec2f(_currentX, _currentY), depth);
    } else {
        Mat4f tform = _translateLocal ? _fixedTransform.stripScale() : _fixedTransform.extractTranslation();
        Vec3f origin = tform*Vec3f(0.0f);
        Vec3f axis(0.0f), normal(0.0f);
        axis[_fixedAxis] = 1.0f;
        normal[(_fixedAxis + 1) % 3] = 1.0f;

        Vec2f p0 = project(origin);
        Vec2f p1 = project(tform*axis);
        Vec2f v = (p1 - p0).normalized();
        Vec2f q0 = v.dot(Vec2f(  _beginX,   _beginY) - p0)*v + p0;
        Vec2f q1 = v.dot(Vec2f(_currentX, _currentY) - p0)*v + p0;
        begin   = intersectPlane(q0, origin, tform.transformVector(normal));
        current = intersectPlane(q1, origin, tform.transformVector(normal));
    }
    Vec3f delta = current - begin;
    if (_snapToGrid) {
        if (_fixedAxis != -1 && _translateLocal)
            delta = snapLength(delta, 0.1f);
        else
            delta = snapToGrid(delta, 0.1f);
    }
    _deltaTransform = Mat4f::translate(delta);
}

void TransformGizmo::computeRotation()
{
//  if (_fixedAxis != -1) {
//      Vec3f origin = _fixedTransform*Vec3f(0.0f);
//      Vec3f n(0.0f), u(0.0f), v(0.0f);
//      n[ _fixedAxis         ] = 1.0f;
//      u[(_fixedAxis + 1) % 3] = 1.0f;
//      v[(_fixedAxis + 2) % 3] = 1.0f;
//
//      Vec3f pa = intersectPlane(Vec2f(  _beginX,   _beginY), origin, n) - origin;
//      Vec3f pb = intersectPlane(Vec2f(_currentX, _currentY), origin, n) - origin;
//      Vec2f a(pa.dot(u), pa.dot(v));
//      Vec2f b(pb.dot(u), pb.dot(v));
//      Vec3f delta(0.0f);
//      delta[_fixedAxis] = Angle::radToDeg(std::atan2(b.y(), b.x()) - std::atan2(a.y(), a.x()));
//      _deltaTransform = Mat4f::translate(origin)*Mat4f::rotXYZ(delta)*Mat4f::translate(-origin);
//  } else {
//      Vec2f origin = project(_fixedTransform*Vec3f(0.0f));
//      Vec2f a(  _beginX - origin.x(),   _beginY - origin.y());
//      Vec2f b(_currentX - origin.x(), _currentY - origin.y());
//      float delta = std::atan2(-b.y(), b.x()) - std::atan2(-a.y(), a.x());
//      //_deltaTransform = Mat4f::translate(origin)*Mat4f::rotXYZ(delta)*Mat4f::translate(-origin);
//      _deltaTransform = _view*Mat4f::rotXYZ(Vec3f(0.0f, 0.0f, Angle::radToDeg(delta)))*_invView;
//  }
    Vec3f worldOrigin = _fixedTransform*Vec3f(0.0f);
    Vec3f viewOrigin = _invView*worldOrigin;
    Vec2f screenOrigin = project(worldOrigin);
    Vec2f a = Vec2f(_beginX, _beginY) - screenOrigin;
    Vec2f b = Vec2f(_currentX, _currentY) - screenOrigin;
    float angle = -Angle::radToDeg(std::atan2(b.y(), b.x()) - std::atan2(a.y(), a.x()));
    if (_snapToGrid)
        angle = snapToGrid(angle, 5.0f);
    if (_fixedAxis != -1) {
        Mat4f toLocal = _fixedTransform.extractRotation().transpose()*_view;
        if (toLocal.transformVector(Vec3f(0.0f, 0.0f, 1.0f))[_fixedAxis] > 0.0f)
            angle *= -1;
        Vec3f delta(0.0f);
        delta[_fixedAxis] = angle;
        _deltaTransform = _fixedTransform.stripScale()*Mat4f::rotXYZ(delta)*_fixedTransform.stripScale().pseudoInvert();
    } else {
        _deltaTransform = _view*Mat4f::translate(viewOrigin)*
                          Mat4f::rotXYZ(Vec3f(0.0f, 0.0f, angle))*
                          Mat4f::translate(-viewOrigin)*_invView;
    }
}

void TransformGizmo::computeScale()
{
    Vec2f begin(_beginX, _beginY);
    Vec2f current(_currentX, _currentY);
    Vec2f origin = project(_fixedTransform*Vec3f(0.0f));
    float factor = (current - origin).length()/(begin - origin).length();
    if (_snapToGrid)
        factor = snapToGrid(factor, 0.1f);
    Vec3f scale(1.0f);
    if (_fixedAxis == -1)
        scale = Vec3f(factor);
    else
        scale[_fixedAxis] = sgn((current - origin).dot(begin - origin))*factor;
    _deltaTransform = _fixedTransform.stripScale()*Mat4f::scale(scale)*_fixedTransform.stripScale().pseudoInvert();
}

void TransformGizmo::drawStatic(AbstractPainter &painter)
{
    Mat4f tform = _deltaTransform*_fixedTransform.stripScale();
    float depth = (_invView*tform*Vec3f(0.0f)).z();
    if (depth < 0.0f)
        return;

    Vec2f begin(_beginX, _beginY);
    Vec2f current(_currentX, _currentY);
    Vec2f origin = project(_deltaTransform*_fixedTransform*Vec3f(0.0f));

    Mat4f proj = (_invView*_deltaTransform*_fixedTransform).extractRotation();
    Vec3f x = proj.right()*Vec3f(1.0f, -1.0f, 1.0f);
    Vec3f y = proj.   up()*Vec3f(1.0f, -1.0f, 1.0f);
    Vec3f z = proj.  fwd()*Vec3f(1.0f, -1.0f, 1.0f);
    Vec3f colors[] = {
        Vec3f(1.0f, 0.0f, 0.0f),
        Vec3f(0.0f, 1.0f, 0.0f),
        Vec3f(0.0f, 0.0f, 1.0f),
    };
    float add[4] = {0.0f};
    if (_hoverShape >= 0)
        add[_hoverShape] = 1.0f;

    painter.setColor(Vec3f(0.0f));
    painter.drawEllipse(origin, Vec2f(4.0f), true);
    painter.setColor(Vec3f(0.61f, 0.3f, 0.07f));
    painter.drawEllipse(origin, Vec2f(3.0f), true);

    switch (_mode) {
    case MODE_TRANSLATE: {
        if (!_translateLocal)
            tform = _deltaTransform*_fixedTransform.extractTranslation();
        painter.labelShape(3);
        painter.setColor(Vec3f(1.0f));
        painter.drawEllipse(origin, Vec2f(10.0f), false, 2.0f + add[3]);
        for (int i = 0; i < 3; ++i) {
            Vec3f p0(0.0f), p1(0.0f);
            p0[i] = depth*0.015f;
            p1[i] = depth*0.1f;
            Vec2f q0 = project(tform*p0);
            Vec2f q1 = project(tform*p1);
            float alpha = smoothStep(4.0f, 20.0f, (origin - q1).length());
            painter.labelShape(alpha > 0.5f ? i : -1);
            painter.setColor(colors[i]);
            painter.setAlpha(alpha);
            painter.drawLine(q0, q1, 2.0f + add[i]);
            painter.drawEllipse(q1, Vec2f(4.0f), true);
        }
        painter.setAlpha(1.0f);
        break;
    } case MODE_ROTATE: {
        painter.setColor(Vec3f(0.0f, 0.0f, 0.0f));
        painter.drawEllipse(origin, Vec2f(90.0f), false, 1.5f);
        painter.labelShape(3);
        painter.setColor(Vec3f(0.0f, 1.0f, 1.0f));
        painter.drawEllipse(origin, Vec2f(100.0f), false, 2.0f + add[3]);
        Vec3f u[] = {y, z, x};
        Vec3f v[] = {z, x, y};
        for (int i = 0; i < 3; ++i) {
            float a = std::atan2(1.0f, -v[i].z()/u[i].z()) + (u[i].z() < 0.0f ? PI : 0.0f);
            painter.labelShape(i);
            painter.setTransform(u[i].xy(), v[i].xy(), origin);
            painter.setColor(colors[i]);
            painter.drawArc(Vec2f(0.0f), Vec2f(90.0f), a, a + PI, false, 3.0f + add[i]);
        }
        break;
    } case MODE_SCALE:
        painter.labelShape(3);
        painter.setColor(Vec3f(1.0f));
        painter.drawRect(origin - Vec2f(10.0f), Vec2f(20.0f), false, 2.0f + add[3]);
        for (int i = 0; i < 3; ++i) {
            Vec3f p0(0.0f), p1(0.0f);
            p0[i] = depth*0.015f;
            p1[i] = depth*0.1f;
            Vec2f q0 = project(tform*p0);
            Vec2f q1 = project(tform*p1);
            float alpha = smoothStep(4.0f, 20.0f, (origin - q1).length());
            painter.labelShape(alpha > 0.5f ? i : -1);
            painter.setColor(colors[i]);
            painter.setAlpha(alpha);
            painter.drawLine(q0, q1, 2.0f + add[i]);
            painter.drawRect(q1 - Vec2f(4.0f), Vec2f(8.0f), true);
        }
        painter.setAlpha(1.0f);
        break;
    }
}

void TransformGizmo::drawDynamic(AbstractPainter &painter)
{
    if (!_transforming)
        return;

    Vec2f begin(_beginX, _beginY);
    Vec2f current(_currentX, _currentY);
    Vec2f oldOrigin = project(_fixedTransform*Vec3f(0.0f));

    switch (_mode) {
    case MODE_TRANSLATE: {
        if (_fixedAxis >= 0 && _fixedAxis < 3) {
            float zNear = 0.1f;//(-_projection.a34 - 1.0f)/_projection.a33;
            float zFar  = 100.0f;//(-_projection.a34 + 1.0f)/_projection.a33;
            Vec3f p(0.0f);
            p[_fixedAxis] = 1.0f;
            Mat4f tform = _translateLocal ? _invView*_fixedTransform : _invView*_fixedTransform.extractTranslation();
            Vec3f p0 = tform*Vec3f(0.0f);
            Vec3f v  = tform.transformVector(p);
            Vec2f q0 = project(_view*((zNear - p0.z())/v.z()*v + p0));
            Vec2f q1 = project(_view*((zFar  - p0.z())/v.z()*v + p0));
            painter.setColor(Vec3f(0.2f));
            painter.drawLine(q0, q1, 2.0f);
        }
        break;
    } case MODE_ROTATE: {
        painter.setColor(Vec3f(0.61f));
        painter.drawLineStipple(current, oldOrigin, 20.0f, 2.0f);
        painter.setColor(Vec4f(0.32f, 0.32f, 0.32f, 0.8f));
        float aStart = std::atan2(   begin.y() - oldOrigin.y(),    begin.x() - oldOrigin.x());
        float aEnd   = std::atan2( current.y() - oldOrigin.y(),  current.x() - oldOrigin.x());
        if (_snapToGrid)
            aEnd = snapToGrid(aEnd - aStart, Angle::degToRad(5.0f)) + aStart;
        if (aStart > aEnd)
            std::swap(aStart, aEnd);
        if (aEnd - aStart > PI) {
            std::swap(aEnd, aStart);
            aEnd += TWO_PI;
        }
        if (aEnd != aStart)
            painter.drawArc(oldOrigin, Vec2f(90.0f), aStart, aEnd, true);
        break;
    } case MODE_SCALE: {
        if (_fixedAxis >= 0 && _fixedAxis < 3) {
            float zNear = 0.1f;//(-_projection.a34 - 1.0f)/_projection.a33;
            float zFar  = 100.0f;//(-_projection.a34 + 1.0f)/_projection.a33;
            Vec3f p(0.0f);
            p[_fixedAxis] = 1.0f;
            Vec3f p0 = (_invView*_fixedTransform)*Vec3f(0.0f);
            Vec3f v  = (_invView*_fixedTransform).transformVector(p);
            Vec2f q0 = project(_view*((zNear - p0.z())/v.z()*v + p0));
            Vec2f q1 = project(_view*((zFar  - p0.z())/v.z()*v + p0));
            painter.setColor(Vec3f(0.2f));
            painter.drawLine(q0, q1, 2.0f);
        }
        painter.setColor(Vec3f(0.61f));
        painter.drawLineStipple(current, oldOrigin, 20.0f, 2.0f);
        break;
    }}
}

void TransformGizmo::beginTransform(int x, int y)
{
    _transforming = true;
    _currentX = _beginX = x;
    _currentY = _beginY = y;
    _fixedAxis = -1;
    _deltaTransform = Mat4f();
}

void TransformGizmo::updateTransform(int x, int y)
{
    _currentX = x;
    _currentY = y;

    switch (_mode) {
    case MODE_TRANSLATE: computeTranslation(); break;
    case MODE_ROTATE:    computeRotation();    break;
    case MODE_SCALE:     computeScale();       break;
    }
}

void TransformGizmo::endTransform()
{
    _transforming = false;
    _needsMouseDown = false;
    Mat4f delta = _deltaTransform;
    _deltaTransform = Mat4f();
    emit transformFinished(delta);
}

void TransformGizmo::abortTransform()
{
    _transforming = false;
    _needsMouseDown = false;
    _deltaTransform = Mat4f();
}

bool TransformGizmo::update(const QMouseEvent *event)
{
    if (_transforming) {
        if (event->buttons() != Qt::NoButton && event->buttons() != Qt::LeftButton)
            abortTransform();
        else if (bool(event->buttons() & Qt::LeftButton) != _needsMouseDown)
            endTransform();
        else
            updateTransform(event->pos().x(), event->pos().y());

        return event->buttons() != 0;
    } else {
        ShapeInput input(Vec2f(float(event->pos().x()), float(event->pos().y())), 10.0f);
        drawStatic(input);

        _hoverShape = input.closestShape();
        if (_hoverShape != -1) {
            if (event->buttons() & Qt::LeftButton) {
                beginTransform(event->pos().x(), event->pos().y());
                _needsMouseDown = true;
                if (_hoverShape < 3)
                    fixAxis(_hoverShape);
                _hoverShape = -1;
                return true;
            }
        }
    }
    return false;
}

void TransformGizmo::draw()
{
    ShapePainter painter;
    drawDynamic(painter);
    drawStatic(painter);
}

}
