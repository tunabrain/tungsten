#ifndef BSDFDISPLAY_HPP_
#define BSDFDISPLAY_HPP_

#include <QLabel>
#include <memory>

class QImage;

namespace Tungsten {

class TraceableScene;
class Primitive;
class Scene;
class Bsdf;

class BsdfDisplay : public QLabel
{
    Q_OBJECT

    enum PrimitiveMode
    {
        PRIMITIVE_SPHERE,
        PRIMITIVE_CUBE,
        PRIMITIVE_PLANE,
    };

    int _w, _h;
    QImage _image;
    int _spp;

    PrimitiveMode _mode;

    std::unique_ptr<Scene> _scene;
    std::vector<std::shared_ptr<Primitive>> _prims;
    std::unique_ptr<TraceableScene> _flattenedScene;
    std::shared_ptr<Bsdf> _bsdf;

    void rebuildImage();

private slots:
    void finishRender();

public:
    BsdfDisplay(int w, int h, QWidget *parent = nullptr);
    ~BsdfDisplay();

public slots:
    void changeBsdf(std::shared_ptr<Bsdf> &bsdf);

signals:
    void rendererFinished();
};

}

#endif /* BSDFDISPLAY_HPP_ */
