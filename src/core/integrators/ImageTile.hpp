#ifndef IMAGETILE_HPP_
#define IMAGETILE_HPP_

#include "IntTypes.hpp"

#include <memory>

namespace Tungsten {

class SampleGenerator;

struct ImageTile
{
    uint32 x, y, w, h;
    std::unique_ptr<SampleGenerator> sampler;
    std::unique_ptr<SampleGenerator> supplementalSampler;

    ImageTile(ImageTile &&o)
    {
        x = o.x;
        y = o.y;
        w = o.w;
        h = o.h;
        sampler = std::move(o.sampler);
        supplementalSampler = std::move(o.supplementalSampler);
    }

    ImageTile(uint32 x_, uint32 y_, uint32 w_, uint32 h_,
            std::unique_ptr<SampleGenerator> sampler_,
            std::unique_ptr<SampleGenerator> supplementalSampler_)
    :   x(x_), y(y_), w(w_), h(h_),
        sampler(std::move(sampler_)),
        supplementalSampler(std::move(supplementalSampler_))
    {
    }
};

}

#endif /* IMAGETILE_HPP_ */
