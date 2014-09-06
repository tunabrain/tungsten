#ifndef IMAGETILE_HPP_
#define IMAGETILE_HPP_

#include "IntTypes.hpp"

#include <memory>

namespace Tungsten {

class SampleGenerator;
class UniformSampler;

struct ImageTile
{
    uint32 x, y, w, h;
    std::unique_ptr<SampleGenerator> sampler;
    std::unique_ptr<UniformSampler> supplementalSampler;

    ImageTile(uint32 x_, uint32 y_, uint32 w_, uint32 h_,
            std::unique_ptr<SampleGenerator> sampler_,
            std::unique_ptr<UniformSampler> supplementalSampler_)
    :   x(x_), y(y_), w(w_), h(h_),
        sampler(std::move(sampler_)),
        supplementalSampler(std::move(supplementalSampler_))
    {
    }
};

}

#endif /* IMAGETILE_HPP_ */
