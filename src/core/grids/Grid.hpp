#ifndef GRID_HPP_
#define GRID_HPP_

#include "math/Mat4f.hpp"
#include "math/Box.hpp"

#include "io/JsonSerializable.hpp"

namespace Tungsten {

class PathSampleGenerator;

class Grid : public JsonSerializable
{
public:
    virtual ~Grid() {}

    virtual Mat4f naturalTransform() const;
    virtual Mat4f invNaturalTransform() const;
    virtual Box3f bounds() const;

    virtual float density(Vec3f p) const = 0;
    virtual Vec3f transmittance(PathSampleGenerator &sampler, Vec3f p, Vec3f w, float t0, float t1, Vec3f sigmaT) const = 0;
    virtual Vec2f inverseOpticalDepth(PathSampleGenerator &sampler, Vec3f p, Vec3f w, float t0, float t1,
            float sigmaT, float xi) const = 0;
};

}

#endif /* GRID_HPP_ */
