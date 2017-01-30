#include "TextureDisplay.hpp"

#include "textures/Texture.hpp"

#include "math/MathUtil.hpp"

#include <QColorDialog>

namespace Tungsten {

TextureDisplay::TextureDisplay(int w, int h, QWidget *parent)
: QLabel(parent),
  _w(w),
  _h(h),
  _tex(nullptr),
  _image(w, h, QImage::Format_RGB888)
{
    setMinimumSize(_w, _h);
    setMaximumSize(_w, _h);

    rebuildImage();
}

void TextureDisplay::rebuildImage()
{
    if (!_tex) {
        _image.fill(Qt::black);
    } else {
        for (int y = 0; y < _h; ++y) {
            Vec3c *img = reinterpret_cast<Vec3c *>(_image.scanLine(y));
            for (int x = 0; x < _w; ++x) {
                Vec2f uv((x + 0.5f)/_w, 1.0f - (y + 0.5f)/_h);
                Vec3f color = std::pow((*_tex)[uv], 1.0f/2.2f);
                img[x] = Vec3c(clamp(Vec3i(color*255.0f), Vec3i(0), Vec3i(255)));
            }
        }
    }

    setBackgroundRole(QPalette::Base);
    setPixmap(QPixmap::fromImage(_image));
}

void TextureDisplay::changeTexture(Texture *tex)
{
    _tex = tex;
    rebuildImage();
}

}
