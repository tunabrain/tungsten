#include "RenderWindow.hpp"
#include "MainWindow.hpp"

#include "io/DirectoryChange.hpp"
#include "io/FileUtils.hpp"

#include <string.h>
#include <QTimer>
#include <QtWidgets>

namespace Tungsten {

RenderWindow::RenderWindow(QWidget *proxyParent, MainWindow *parent)
: QWidget(proxyParent),
  _parent(*parent),
  _scene(nullptr),
  _rendering(false),
  _autoRefresh(false),
  _zoom(1.0f),
  _panX(0.0f),
  _panY(0.0f),
  _exposure(0.0f),
  _pow2Exposure(1.0f),
  _gamma(2.2f)
{
    connect(this, SIGNAL(rendererFinished()), SLOT(finishRender()), Qt::QueuedConnection);

    new QShortcut(QKeySequence("Space"), this, SLOT(toggleRender()));
    new QShortcut(QKeySequence("+"), this, SLOT(zoomIn()));
    new QShortcut(QKeySequence("-"), this, SLOT(zoomOut()));
    new QShortcut(QKeySequence("F5"), this, SLOT(refresh()));
    new QShortcut(QKeySequence("R"), this, SLOT(toggleAutoRefresh()));
    new QShortcut(QKeySequence("Home"), this, SLOT(resetView()));
    new QShortcut(QKeySequence("Ctrl+Tab"), this, SLOT(togglePreview()));

    _sppLabel = new QLabel(this);
    _statusLabel = new QLabel(this);

    _sppLabel->setMinimumWidth(100);
}

void RenderWindow::addStatusWidgets(QStatusBar *statusBar)
{
    statusBar->addPermanentWidget(_sppLabel, 0);
    statusBar->addPermanentWidget(_statusLabel, 1);
}

void RenderWindow::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.fillRect(0, 0, width(), height(), QColor(Qt::darkGray));

    if (!_image)
        return;

    int offsetX =  width()/2 + (_panX - _image-> width()/2)*_zoom;
    int offsetY = height()/2 + (_panY - _image->height()/2)*_zoom;

    painter.translate(QPoint(offsetX, offsetY));
    painter.scale(_zoom, _zoom);

    QRect exposedRect = painter.matrix().inverted()
                 .mapRect(event->rect())
                 .adjusted(-1, -1, 1, 1);
    painter.drawImage(exposedRect, *_image, exposedRect);
}

void RenderWindow::mouseMoveEvent(QMouseEvent *eventMove)
{
    _panX += (eventMove->pos().x() - _lastMousePos.x())/_zoom;
    _panY += (eventMove->pos().y() - _lastMousePos.y())/_zoom;
    _lastMousePos = eventMove->pos();

    update();
}

void RenderWindow::mousePressEvent(QMouseEvent *eventMove)
{
    _lastMousePos = eventMove->pos();
}

void RenderWindow::wheelEvent(QWheelEvent *wheelEvent)
{
    if (wheelEvent->delta() < 0)
        zoomOut();
    else
        zoomIn();
}

void RenderWindow::sceneChanged()
{
    _scene = _parent.scene();
    if (_flattenedScene)
        _flattenedScene->integrator().abortRender();
    _flattenedScene.reset();
    _rendering = false;

    if (_scene) {
        Vec2u resolution = _scene->camera()->resolution();
        _image.reset(new QImage(QSize(resolution.x(), resolution.y()), QImage::Format_ARGB32));
        _image->fill(Qt::black);
        repaint();
    }
}

QRgb RenderWindow::tonemap(const Vec3f &c) const
{
    Vec3i pixel(clamp(c*_pow2Exposure*255.0f, Vec3f(0.0f), Vec3f(255.0f)));

    return qRgb(pixel.x(), pixel.y(), pixel.z());
}

void RenderWindow::updateStatus()
{
    if (_scene) {
        int currentSpp = _flattenedScene ?
                  _flattenedScene->integrator().currentSpp()
                : _scene->rendererSettings().spp();

        _sppLabel->setText(QString("%1/%2 spp").arg(currentSpp).arg(_scene->rendererSettings().spp()));
        if (_rendering)
            _statusLabel->setText("Rendering...");
        else
            _statusLabel->setText("Render finished");
    } else {
        _sppLabel->setText("");
        _statusLabel->setText("");
    }
}

void RenderWindow::startRender()
{
    if (!_scene)
        return;

    if (!_flattenedScene) {
        _flattenedScene.reset(_scene->makeTraceable());

        _image->fill(Qt::black);
        repaint();
    }

    auto finishCallback = [&]() {
        emit rendererFinished();
    };

    _rendering = true;
    updateStatus();

    _flattenedScene->integrator().startRender(finishCallback);
}

void RenderWindow::abortRender()
{
    _rendering = false;
    if (_flattenedScene) {
        _flattenedScene->integrator().abortRender();

        refresh();
        updateStatus();

        _flattenedScene.reset();
    }
}

void RenderWindow::finishRender()
{
    if (_flattenedScene)
        _flattenedScene->integrator().waitForCompletion();
    if (!_rendering)
        return;
    _rendering = false;
    refresh();

    if (_scene && !_flattenedScene->integrator().done())
        startRender();
    else {
        if (_scene) {
            DirectoryChange context(_scene->path().parent());
            _flattenedScene->integrator().saveOutputs();
        }
        _flattenedScene.reset();

        updateStatus();
    }
}

void RenderWindow::refresh()
{
    if (!_image || !_flattenedScene)
        return;

    uint32 w = _image->width(), h = _image->height();
    QRgb *pixels = reinterpret_cast<QRgb *>(_image->bits());

    for (uint32 y = 0, idx = 0; y < h; ++y)
        for (uint32 x = 0; x < w; ++x, ++idx)
            pixels[idx] = tonemap(_scene->camera()->get(x, y));

    update();

    if (_autoRefresh)
        QTimer::singleShot(2000, this, SLOT(refresh()));
}

void RenderWindow::toggleRender()
{
    if (_rendering)
        abortRender();
    else
        startRender();
}

void RenderWindow::zoomIn()
{
    _zoom = clamp(_zoom*1.25f, 0.05f, 20.0f);
    update();
}

void RenderWindow::zoomOut()
{
    _zoom = clamp(_zoom/1.25f, 0.05f, 20.0f);
    update();
}

void RenderWindow::resetView()
{
    _zoom = 1.0f;
    _panX = _panY = 0.0f;
    update();
}

void RenderWindow::togglePreview()
{
    if (!_rendering)
        _parent.togglePreview();
}

void RenderWindow::toggleAutoRefresh()
{
    _autoRefresh = !_autoRefresh;
    if (_autoRefresh)
        QTimer::singleShot(2000, this, SLOT(refresh()));
}

}
