#include "ReconstructionFilter.hpp"

namespace Tungsten {

DEFINE_STRINGABLE_ENUM(ReconstructionFilter::Type, "reconstruction filter", ({
    {"dirac", ReconstructionFilter::Dirac},
    {"box", ReconstructionFilter::Box},
    {"tent", ReconstructionFilter::Tent},
    {"gaussian", ReconstructionFilter::Gaussian},
    {"mitchell_netravali", ReconstructionFilter::MitchellNetravali},
    {"catmull_rom", ReconstructionFilter::CatmullRom},
    {"lanczos", ReconstructionFilter::Lanczos}
}))

float ReconstructionFilter::filterWidth(TypeEnum type)
{
    switch (type) {
    case Dirac:
        return 0.0f;
    case Box:
        return 0.5f;
    case Tent:
        return 1.0f;
    case Gaussian:
    case MitchellNetravali:
    case CatmullRom:
    case Lanczos:
        return 2.0f;
    default:
        return 0.0f;
    }
}

void ReconstructionFilter::precompute()
{
    _width = filterWidth(_type);
    _binSize = _width/RFILTER_RESOLUTION;
    _invBinSize = RFILTER_RESOLUTION/_width;

    if (_type == Box || _type == Dirac)
        return;

    float filterSum = 0.0f;
    for (int i = 0; i < RFILTER_RESOLUTION; ++i) {
        _filter[i] = eval((i*_width)/RFILTER_RESOLUTION);
        filterSum += _filter[i];
    }
    _filter[RFILTER_RESOLUTION] = 0.0f;

    _cdf[0] = 0.0f;
    for (int i = 1; i < RFILTER_RESOLUTION; ++i)
        _cdf[i] = _cdf[i - 1] + _filter[i - 1]/filterSum;
    _cdf[RFILTER_RESOLUTION] = 1.0f;

    float normalizationFactor = filterSum*2.0f*_width/RFILTER_RESOLUTION;
    for (int i = 0; i < RFILTER_RESOLUTION; ++i)
        _filter[i] /= normalizationFactor;
}

}
