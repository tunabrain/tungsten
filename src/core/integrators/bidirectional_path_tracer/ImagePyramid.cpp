#include "ImagePyramid.hpp"

#include "cameras/Camera.hpp"

#include "io/ImageIO.hpp"

namespace Tungsten {

ImagePyramid::ImagePyramid(int maxPathLength, const Camera &camera)
: _camera(camera),
  _maxPathLength(min(maxPathLength, MaxLength)),
  _w(camera.resolution().x()),
  _h(camera.resolution().y()),
  _outBuffer(new Vec3c[_w*_h])
{
    for (int i = 0; i < pyramidCount(_maxPathLength); ++i)
        _frames.emplace_back(_w, _h, camera.reconstructionFilter());
}

void ImagePyramid::saveBuffers(const Path &prefix, int spp, bool uniformWeights)
{
    float splatWeight = 1.0f/(_w*_h*spp);
    float directWeight = 1.0f/spp;
    if (uniformWeights)
        splatWeight = directWeight;

    for (int length = 1; length <= _maxPathLength; ++length) {
        for (int t = 1; t <= length + 1; ++t) {
            int s = length + 1 - t;
            int idx = pyramidIndex(s, t);

            float weight = (length + 1.0f)*(t == 1 ? splatWeight : directWeight);

            for (uint32 i = 0; i < _w*_h; ++i)
                _outBuffer[i] = Vec3c(clamp(Vec3i(_camera.tonemap(_frames[idx].get(i, 0)*weight)*255.0f), Vec3i(0), Vec3i(255)));
            ImageIO::saveLdr(prefix + tfm::format("-s=%d-t=%d.png", s, t), &_outBuffer[0].x(), _w, _h, 3);
        }
    }
}

}
