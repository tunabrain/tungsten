#ifndef MESHINPUTOUTPUT_HPP_
#define MESHINPUTOUTPUT_HPP_

#include <fstream>
#include <string>
#include <vector>

#include "FileUtils.hpp"

#include "IntTypes.hpp"

#include "primitives/Triangle.hpp"
#include "primitives/Vertex.hpp"

#include "math/Vec.hpp"

namespace Tungsten {

class MeshIO
{
public:
    static void load(const std::string &path, std::vector<Vertex> &verts, std::vector<TriangleI> &tris)
    {
        std::ifstream stream(path, std::ios_base::in | std::ios_base::binary);

        if (!stream.good())
            return;

        uint64 numVerts, numTris;
        FileUtils::streamRead(stream, numVerts);
        verts.resize(numVerts);
        FileUtils::streamRead(stream, verts);
        FileUtils::streamRead(stream, numTris);
        tris.resize(numTris);
        FileUtils::streamRead(stream, tris);
    }

    static void save(const std::string &path, const std::vector<Vertex> &verts, const std::vector<TriangleI> &tris)
    {
        std::ofstream stream(path, std::ios_base::out | std::ios_base::binary);

        if (!stream.good())
            return;

        FileUtils::streamWrite(stream, uint64(verts.size()));
        FileUtils::streamWrite(stream, verts);
        FileUtils::streamWrite(stream, uint64(tris.size()));
        FileUtils::streamWrite(stream, tris);
    }
};

}

#endif /* MESHINPUTOUTPUT_HPP_ */
