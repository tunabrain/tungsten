#ifndef IMAGETILE_HPP_
#define IMAGETILE_HPP_

#include "IntTypes.hpp"

#include <memory>

namespace Tungsten {

class PathSampleGenerator;

struct ImageTile
{
    uint32 x, y, w, h;
    std::unique_ptr<PathSampleGenerator> sampler;

    ImageTile(ImageTile &&o)
    {
        x = o.x;
        y = o.y;
        w = o.w;
        h = o.h;
        sampler = std::move(o.sampler);
    }

    ImageTile(uint32 x_, uint32 y_, uint32 w_, uint32 h_,
            std::unique_ptr<PathSampleGenerator> sampler_)
    :   x(x_), y(y_), w(w_), h(h_),
        sampler(std::move(sampler_))
    {
    }
};

}

#endif /* IMAGETILE_HPP_ */
