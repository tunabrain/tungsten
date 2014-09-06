#ifndef TRANSFORMGIZMO_HPP_
#define TRANSFORMGIZMO_HPP_

#include <QObject>

#include "math/Mat4f.hpp"
#include "math/Vec.hpp"

class QMouseEvent;

namespace Tungsten {

class AbstractPainter;

class TransformGizmo : public QObject
{
    Q_OBJECT

    enum TransformMode
    {
        MODE_TRANSLATE,
        MODE_ROTATE,
        MODE_SCALE,
    };

    TransformMode _mode;
    bool _needsMouseDown;
    bool _translateLocal;
    bool _snapToGrid;
    bool _transforming;
    float _beginX;
    float _beginY;
    float _currentX;
    float _currentY;
    int _fixedAxis;

    int _w;
    int _h;
    Mat4f _invView;
    Mat4f _view;
    Mat4f _projection;

    Mat4f _deltaTransform;
    Mat4f _fixedTransform;

    int _hoverShape;

    Vec2f project(const Vec3f &p) const;
    Vec3f viewVector(const Vec2f& p) const;
    Vec3f unproject(const Vec2f &p, float depth) const;
    Vec3f intersectPlane(const Vec2f &p, const Vec3f &base, const Vec3f &n) const;
    Vec2f projectAxis(const Mat4f &mat, int axis) const;
    float relativeMovement(int axis, const Vec2f &move) const;

    void computeTranslation();
    void computeRotation();
    void computeScale();

    void drawStatic(AbstractPainter &painter);
    void drawDynamic(AbstractPainter &painter);

    template<typename T>
    T snapToGrid(T p, float t) const
    {
        return std::trunc(p/t)*t;
    }

    Vec3f snapLength(const Vec3f p, float t) const
    {
        if (p.length() == 0.0f)
            return Vec3f(0.0f);
        else
            return p.normalized()*snapToGrid(p.length(), t);
    }

public:
    TransformGizmo();

    bool transforming() const
    {
        return _transforming;
    }

    const Mat4f &deltaTransform() const
    {
        return _deltaTransform;
    }

signals:
    void transformFinished(Mat4f delta);
    void redraw();

public slots:
    void setMode(int mode)
    {
        _mode = TransformMode(mode);
        emit redraw();
    }

    void toggleTranslateLocal()
    {
        _translateLocal = !_translateLocal;
        emit redraw();
    }

    void resize(int w, int h)
    {
        _w = w;
        _h = h;
    }

    void fixAxis(int axis)
    {
        _fixedAxis = axis;
        updateTransform(_currentX, _currentY);
        emit redraw();
    }

    void setView(const Mat4f &m)
    {
        _view = m;
        _invView = _view.pseudoInvert();
    }

    void setProjection(const Mat4f &m)
    {
        _projection = m;
    }

    void setFixedTransform(const Mat4f &m)
    {
        _fixedTransform = m;
    }

    void beginTransform(int x, int y);
    void updateTransform(int x, int y);
    void endTransform();
    void abortTransform();

    void setSnapToGrid(bool v)
    {
        _snapToGrid = v;
        if (_transforming) {
            updateTransform(_currentX, _currentY);
            emit redraw();
        }
    }

    bool update(const QMouseEvent *event);

    void draw();
};

}

#endif /* TRANSFORMGIZMO_HPP_ */
