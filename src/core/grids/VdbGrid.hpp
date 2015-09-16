#ifndef VDBGRID_HPP_
#define VDBGRID_HPP_

#if OPENVDB_AVAILABLE

#include "Grid.hpp"

#include "io/FileUtils.hpp"

#include <openvdb/openvdb.h>

namespace Tungsten {

class VdbGrid : public Grid
{
    PathPtr _path;
    std::string _gridName;
    float _stepSize;
    Mat4f _configTransform;
    Mat4f _invConfigTransform;

    openvdb::FloatGrid::Ptr _grid;
    Mat4f _transform;
    Mat4f _invTransform;
    Box3f _bounds;

public:
    VdbGrid();

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual void loadResources() override;

    virtual Mat4f naturalTransform() const override;
    virtual Mat4f invNaturalTransform() const override;
    virtual Box3f bounds() const override;

    float density(Vec3f p) const override;
    float densityIntegral(PathSampleGenerator &sampler, Vec3f p, Vec3f w, float t0, float t1) const override;
    Vec2f inverseOpticalDepth(PathSampleGenerator &sampler, Vec3f p, Vec3f w, float t0, float t1,
            float sigmaT, float xi) const override;
};

}

#endif

#endif /* VDBGRID_HPP_ */
