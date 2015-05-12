#ifndef SURFACERECORD_HPP_
#define SURFACERECORD_HPP_

#include "samplerecords/SurfaceScatterEvent.hpp"

#include "primitives/IntersectionTemporary.hpp"
#include "primitives/IntersectionInfo.hpp"

namespace Tungsten {

struct SurfaceRecord
{
    SurfaceScatterEvent event;
    IntersectionTemporary data;
    IntersectionInfo info;
};

}

#endif /* SURFACERECORD_HPP_ */
