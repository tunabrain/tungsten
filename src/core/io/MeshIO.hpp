#ifndef MESHINPUTOUTPUT_HPP_
#define MESHINPUTOUTPUT_HPP_

#include "primitives/Triangle.hpp"
#include "primitives/Vertex.hpp"

#include <string>
#include <vector>

namespace Tungsten {

namespace MeshIO {

bool load(const std::string &path, std::vector<Vertex> &verts, std::vector<TriangleI> &tris);
bool save(const std::string &path, const std::vector<Vertex> &verts, const std::vector<TriangleI> &tris);

}

}

#endif /* MESHINPUTOUTPUT_HPP_ */
