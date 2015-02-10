#ifndef MESHINPUTOUTPUT_HPP_
#define MESHINPUTOUTPUT_HPP_

#include "Path.hpp"

#include "primitives/Triangle.hpp"
#include "primitives/Vertex.hpp"

#include <string>
#include <vector>

namespace Tungsten {

namespace MeshIO {

bool load(const Path &path, std::vector<Vertex> &verts, std::vector<TriangleI> &tris);
bool save(const Path &path, const std::vector<Vertex> &verts, const std::vector<TriangleI> &tris);

}

}

#endif /* MESHINPUTOUTPUT_HPP_ */
