#ifndef CAMERARECORD_HPP_
#define CAMERARECORD_HPP_

#include "samplerecords/DirectionSample.hpp"
#include "samplerecords/PositionSample.hpp"

#include "math/Vec.hpp"

namespace Tungsten {

struct CameraRecord
{
    Vec2u pixel;
    PositionSample point;
    DirectionSample direction;

    CameraRecord() = default;
    CameraRecord(Vec2u pixel_)
    : pixel(pixel_)
    {
    }
};

}

#endif /* CAMERARECORD_HPP_ */
