#include "BsdfDisplay.hpp"

#include "renderer/TraceableScene.hpp"

#include "primitives/InfiniteSphere.hpp"
#include "primitives/Sphere.hpp"
#include "primitives/Cube.hpp"
#include "primitives/Quad.hpp"
#include "primitives/Disk.hpp"

#include "textures/CheckerTexture.hpp"

#include "bsdfs/LambertBsdf.hpp"
#include "bsdfs/NullBsdf.hpp"

#include "io/TextureCache.hpp"
#include "io/Scene.hpp"

#include "math/MathUtil.hpp"

namespace Tungsten {

BsdfDisplay::BsdfDisplay(int w, int h, QWidget *parent)
: QLabel(parent),
  _w(w),
  _h(h),
  _image(w, h, QImage::Format_RGB888),
  _spp(64),
  _mode(PRIMITIVE_SPHERE),
  _scene(new Scene())
{
    setMinimumSize(_w, _h);
    setMaximumSize(_w, _h);

    _prims.emplace_back(std::make_shared<Sphere>());
    _prims.emplace_back(std::make_shared<Cube>());
    _prims.emplace_back(std::make_shared<Quad>());

    std::shared_ptr<Cube> floor = std::make_shared<Cube>();
    std::shared_ptr<Disk> light = std::make_shared<Disk>();

    std::shared_ptr<Bsdf> floorBsdf = std::make_shared<LambertBsdf>();
    std::shared_ptr<Bsdf> lightBsdf = std::make_shared<NullBsdf>();

    std::shared_ptr<CheckerTexture> floorTexture = std::make_shared<CheckerTexture>();
    floorTexture->setResU(40);
    floorTexture->setResV(40);
    floorTexture->setOnColor(Vec3f(1.0f));
    floorTexture->setOnColor(Vec3f(0.1f));
    floorBsdf->setAlbedo(floorTexture);

    floor->setTransform(Mat4f::translate(Vec3f(0.0f, 2.748f, 3.0f))*Mat4f::scale(Vec3f(15.0f, 7.5f, 9.0f)));
    light->setTransform(Mat4f::translate(Vec3f(-5.0f, 5.0f, 5.0f))*
            Mat4f::rotYXZ(Vec3f(225.0f, 45.0f, 0.0f)));

    floor->setBsdf(0, floorBsdf);
    light->setBsdf(0, lightBsdf);

    light->setEmission(std::make_shared<ConstantTexture>(100.0f));

    floor->setName("Floor");
    light->setName("Light");

    _scene->addPrimitive(floor);
    _scene->addPrimitive(light);

    _scene->camera()->setTonemapString("filmic");
    _scene->camera()->setTransform(Vec3f(0.0f, 1.5f, 1.5f), Vec3f(0.0f), Vec3f(0.0f, 1.0f, 0.0f));
    _scene->camera()->setResolution(Vec2u(_w, _h));

    connect(this, SIGNAL(rendererFinished()), SLOT(finishRender()), Qt::QueuedConnection);

    rebuildImage();
}

BsdfDisplay::~BsdfDisplay()
{
    if (_flattenedScene)
        _flattenedScene->integrator().abortRender();
}

void BsdfDisplay::rebuildImage()
{
    if (_flattenedScene) {
        for (int y = 0; y < _h; ++y) {
            Vec3c *img = reinterpret_cast<Vec3c *>(_image.scanLine(y));
            for (int x = 0; x < _w; ++x)
                img[x] = Vec3c(clamp(Vec3i(_scene->camera()->get(x, y)*255.0f), Vec3i(0), Vec3i(255)));
        }
    } else {
        _image.fill(Qt::black);
    }

    setBackgroundRole(QPalette::Base);
    setPixmap(QPixmap::fromImage(_image));
}

void BsdfDisplay::finishRender()
{
    if (!_flattenedScene)
        return;

    _flattenedScene->integrator().waitForCompletion();

    rebuildImage();

    if (_flattenedScene->integrator().done())
        _flattenedScene.reset();
    else
        _flattenedScene->integrator().startRender([&]() { emit rendererFinished(); });
}

void BsdfDisplay::changeBsdf(std::shared_ptr<Bsdf> &bsdf)
{
    if (_flattenedScene) {
        _flattenedScene->integrator().abortRender();
        _flattenedScene.reset();
    }

    _bsdf = bsdf;
    rebuildImage();

    if (_scene->primitives().size() > 2)
        _scene->deletePrimitives(std::unordered_set<Primitive *>{_scene->primitives().back().get()});
    _prims[_mode]->setBsdf(0, _bsdf);
    _prims[_mode]->setName("TestMesh");
    _scene->addPrimitive(_prims[_mode]);
    _scene->rendererSettings().setUseSceneBvh(false);
    _scene->rendererSettings().setSpp(_spp);
    _scene->rendererSettings().setSppStep(1);
    _scene->loadResources();
    _flattenedScene.reset(_scene->makeTraceable());

    _flattenedScene->integrator().startRender([&]() { emit rendererFinished(); });
}

}
