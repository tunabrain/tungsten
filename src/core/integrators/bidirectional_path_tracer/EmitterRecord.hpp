#ifndef EMITTERRECORD_HPP_
#define EMITTERRECORD_HPP_

#include "samplerecords/DirectionSample.hpp"
#include "samplerecords/PositionSample.hpp"

namespace Tungsten {

struct EmitterRecord
{
    float emitterPdf;
    PositionSample point;
    DirectionSample direction;

    EmitterRecord() = default;
    EmitterRecord(float emitterPdf_)
    : emitterPdf(emitterPdf_)
    {
    }
};

}

#endif /* EMITTERRECORD_HPP_ */
