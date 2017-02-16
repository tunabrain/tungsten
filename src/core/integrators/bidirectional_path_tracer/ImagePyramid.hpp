#ifndef IMAGEPYRAMID_HPP_
#define IMAGEPYRAMID_HPP_

#include "cameras/AtomicFramebuffer.hpp"

#include <memory>
#include <vector>

namespace Tungsten {

class Camera;
class Path;

class ImagePyramid
{
    const int MaxLength = 12;

    const Camera &_camera;
    int _maxPathLength;
    uint32 _w, _h;
    std::vector<AtomicFramebuffer> _frames;
    std::unique_ptr<Vec3c[]> _outBuffer;

    static inline int pyramidCount(int pathLength)
    {
        return ((pathLength + 1)*(pathLength + 2))/2 - 1;
    }

    static inline int pyramidIndex(int s, int t)
    {
        return pyramidCount(s + t - 2) + t - 1;
    }

public:
    ImagePyramid(int maxPathLength, const Camera &camera);

    inline void splatFiltered(int s, int t, Vec2f pixel, Vec3f w)
    {
        int idx = pyramidIndex(s, t);
        if (idx < 0 || idx >= int(_frames.size()))
            return;
        _frames[idx].splatFiltered(pixel, w);
    }

    inline void splat(int s, int t, Vec2u pixel, Vec3f w)
    {
        int idx = pyramidIndex(s, t);
        if (idx < 0 || idx >= int(_frames.size()))
            return;
        _frames[idx].splat(pixel, w);
    }

    void saveBuffers(const Path &prefix, int spp, bool uniformWeights);
};

}

#endif /* IMAGEPYRAMID_HPP_ */
