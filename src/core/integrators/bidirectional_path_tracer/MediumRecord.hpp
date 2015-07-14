#ifndef MEDIUMRECORD_HPP_
#define MEDIUMRECORD_HPP_

#include "samplerecords/MediumSample.hpp"
#include "samplerecords/PhaseSample.hpp"

namespace Tungsten {

struct MediumRecord
{
    Vec3f wi;
    MediumSample mediumSample;
    PhaseSample phaseSample;
};

}

#endif /* MEDIUMRECORD_HPP_ */
