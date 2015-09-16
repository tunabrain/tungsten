#ifndef VOXELMEDIUM_HPP_
#define VOXELMEDIUM_HPP_

#include "Medium.hpp"

#include "grids/Grid.hpp"

namespace Tungsten {

class VoxelMedium : public Medium
{
    Vec3f _sigmaA, _sigmaS;
    Vec3f _sigmaT;
    bool _absorptionOnly;

    std::shared_ptr<Grid> _grid;

    Mat4f _worldToGrid;
    Box3f _gridBounds;

public:
    VoxelMedium();

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual void loadResources() override;

    virtual bool isHomogeneous() const override;

    virtual void prepareForRender() override;

    virtual bool sampleDistance(PathSampleGenerator &sampler, const Ray &ray,
            MediumState &state, MediumSample &sample) const override;
    virtual Vec3f transmittance(PathSampleGenerator &sampler, const Ray &ray) const override;
    virtual float pdf(PathSampleGenerator &sampler, const Ray &ray, bool onSurface) const override;
    virtual Vec3f transmittanceAndPdfs(PathSampleGenerator &sampler, const Ray &ray, bool startOnSurface,
            bool endOnSurface, float &pdfForward, float &pdfBackward) const override;
};

}

#endif /* VOXELMEDIUM_HPP_ */
