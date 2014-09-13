#ifndef CURVEIO_HPP_
#define CURVEIO_HPP_

#include "math/Vec.hpp"

#include "IntTypes.hpp"

#include <vector>

namespace Tungsten {

namespace CurveIO {

struct CurveData
{
	std::vector<uint32> *curveEnds = nullptr;
	std::vector<Vec4f> *nodeData   = nullptr;
	std::vector<Vec3f> *nodeColor  = nullptr;
	std::vector<Vec3f> *nodeNormal = nullptr;
};

bool load(const std::string &path, CurveData &data);
bool save(const std::string &path, const CurveData &data);

}

}

#endif /* CURVEIO_HPP_ */
