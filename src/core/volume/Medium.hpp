#ifndef MEDIUM_HPP_
#define MEDIUM_HPP_

#include "samplerecords/MediumSample.hpp"

#include "phasefunctions/PhaseFunction.hpp"

#include "math/Ray.hpp"

#include "io/JsonSerializable.hpp"

#include <memory>

namespace Tungsten {

class Scene;

class Medium : public JsonSerializable
{
protected:
    std::shared_ptr<PhaseFunction> _phaseFunction;
    int _maxBounce;

public:
    struct MediumState
    {
        bool firstScatter;
        int component;
        int bounce;

        void reset()
        {
            firstScatter = true;
            bounce = 0;
        }

        void advance()
        {
            firstScatter = false;
            bounce++;
        }
    };

    Medium();

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool isHomogeneous() const = 0;

    virtual void prepareForRender() {}
    virtual void teardownAfterRender() {}

    virtual bool sampleDistance(PathSampleGenerator &sampler, const Ray &ray,
            MediumState &state, MediumSample &sample) const = 0;
    virtual Vec3f transmittance(const Ray &ray) const = 0;
    virtual float pdf(const Ray &ray, bool onSurface) const = 0;
    virtual Vec3f transmittanceAndPdfs(const Ray &ray, bool startOnSurface, bool endOnSurface,
            float &pdfForward, float &pdfBackward) const;
    virtual const PhaseFunction *phaseFunction(const Vec3f &p) const;
};

}



#endif /* MEDIUM_HPP_ */
