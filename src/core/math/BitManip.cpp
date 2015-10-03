#include "BitManip.hpp"

#include <cmath>

namespace Tungsten {

CONSTEXPR uint32 BitManip::LogMantissaBits;
std::unique_ptr<float[]> BitManip::_logLookup;
BitManip::Initializer BitManip::initializer;

BitManip::Initializer::Initializer()
{
    BitManip::_logLookup.reset(new float[1 << BitManip::LogMantissaBits]);
    BitManip::_logLookup[0] = 0.0f;
    for (uint32 i = 1; i < (1 << BitManip::LogMantissaBits); ++i)
        BitManip::_logLookup[i] = std::log2(float(i));
}

}
