#ifndef RENDERWINDOW_HPP_
#define RENDERWINDOW_HPP_

#include <QWidget>
#include <QPixmap>
#include <memory>

#define EXPORT_RAYS 0

#if EXPORT_RAYS
#include "integrators/RayStreamIntegrator.hpp"
#else
#include "integrators/PathTraceIntegrator.hpp"
#endif
#include "TraceableScene.hpp"
#include "Renderer.hpp"

class QStatusBar;
class QLabel;

namespace Tungsten
{


#if EXPORT_RAYS
typedef RayStreamIntegrator RenderIntegrator;
#else
typedef PathTraceIntegrator RenderIntegrator;
#endif

class MainWindow;

class RenderWindow : public QWidget
{
    Q_OBJECT

    MainWindow &_parent;
    Scene *_scene;

    std::unique_ptr<QImage> _image;
    std::unique_ptr<Renderer<RenderIntegrator>> _renderer;
    std::unique_ptr<TraceableScene> _flattenedScene;

    QLabel *_sppLabel, *_statusLabel;

    bool _rendering;
    uint32 _currentSpp, _nextSpp;

    float _zoom;
    QPoint _lastMousePos;
    float _panX, _panY;

    float _exposure, _pow2Exposure;
    float _gamma;

    QRgb tonemap(const Vec3f &c) const;
    uint32 sampleStep(uint32 current, uint32 target) const;

    void updateStatus();

private slots:
    void startRender();
    void abortRender();
    void finishRender();
    void refresh();
    void toggleRender();
    void zoomIn();
    void zoomOut();
    void resetView();
    void togglePreview();

protected:
    virtual void paintEvent(QPaintEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *eventMove);
    virtual void mousePressEvent(QMouseEvent *eventPress);
    virtual void wheelEvent(QWheelEvent *wheelEvent);

public slots:
    void sceneChanged();

signals:
    void rendererFinished();

public:
    RenderWindow(QWidget *proxyParent, MainWindow *parent);

    void addStatusWidgets(QStatusBar *statusBar);
};

}

#endif /* RENDERWINDOW_HPP_ */
