#include "PrecomputedAzimuthalLobe.hpp"

namespace Tungsten {

CONSTEXPR int PrecomputedAzimuthalLobe::AzimuthalResolution;

PrecomputedAzimuthalLobe::PrecomputedAzimuthalLobe(std::unique_ptr<Vec3f[]> table)
: _table(std::move(table))
{
    const int Size = AzimuthalResolution;

    std::vector<float> weights(Size*Size);
    for (int i = 0; i < Size*Size; ++i)
        weights[i] = _table[i].max();

    // Dilate weights slightly to stay conservative
    for (int y = 0; y < Size; ++y) {
        for (int x = 0; x < Size - 1; ++x)
            weights[x + y*Size] = max(weights[x + y*Size], weights[x + 1 + y*Size]);
        for (int x = Size - 1; x > 0; --x)
            weights[x + y*Size] = max(weights[x + y*Size], weights[x - 1 + y*Size]);
    }
    for (int x = 0; x < Size; ++x) {
        for (int y = 0; y < Size - 1; ++y)
            weights[x + y*Size] = max(weights[x + y*Size], weights[x + (y + 1)*Size]);
        for (int y = Size - 1; y > 0; --y)
            weights[x + y*Size] = max(weights[x + y*Size], weights[x + (y - 1)*Size]);
    }

    _sampler.reset(new InterpolatedDistribution1D(std::move(weights), Size, Size));
}

}
