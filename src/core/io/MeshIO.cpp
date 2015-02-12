#include "MeshIO.hpp"
#include "FileUtils.hpp"
#include "ObjLoader.hpp"
#include "IntTypes.hpp"

#include <tinyformat/tinyformat.hpp>

namespace Tungsten {

namespace MeshIO {

bool loadWo3(const Path &path, std::vector<Vertex> &verts, std::vector<TriangleI> &tris)
{

    InputStreamHandle stream = FileUtils::openInputStream(path);
    if (!stream)
        return false;

    uint64 numVerts, numTris;
    FileUtils::streamRead(stream, numVerts);
    verts.resize(size_t(numVerts));
    FileUtils::streamRead(stream, verts);
    FileUtils::streamRead(stream, numTris);
    tris.resize(size_t(numTris));
    FileUtils::streamRead(stream, tris);

    return true;
}

bool saveWo3(const Path &path, const std::vector<Vertex> &verts, const std::vector<TriangleI> &tris)
{
    OutputStreamHandle stream = FileUtils::openOutputStream(path);
    if (!stream)
        return false;

    FileUtils::streamWrite(stream, uint64(verts.size()));
    FileUtils::streamWrite(stream, verts);
    FileUtils::streamWrite(stream, uint64(tris.size()));
    FileUtils::streamWrite(stream, tris);

    return true;
}

bool loadObj(const Path &path, std::vector<Vertex> &verts, std::vector<TriangleI> &tris)
{
    return ObjLoader::loadGeometryOnly(path, verts, tris);
}

bool saveObj(const Path &path, const std::vector<Vertex> &verts, const std::vector<TriangleI> &tris)
{
    OutputStreamHandle stream = FileUtils::openOutputStream(path);
    if (!stream)
        return false;

    for (const Vertex &v : verts)
        tfm::format(*stream, "v %f %f %f\n", v.pos().x(), v.pos().y(), v.pos().z());
    for (const Vertex &v : verts)
        tfm::format(*stream, "vn %f %f %f\n", v.normal().x(), v.normal().y(), v.normal().z());
    for (const Vertex &v : verts)
        tfm::format(*stream, "vt %f %f\n", v.uv().x(), v.uv().y());
    for (const TriangleI &t : tris)
        tfm::format(*stream, "f %d/%d/%d %d/%d/%d %d/%d/%d\n",
            t.v0 + 1, t.v0 + 1, t.v0 + 1,
            t.v1 + 1, t.v1 + 1, t.v1 + 1,
            t.v2 + 1, t.v2 + 1, t.v2 + 1);

    return true;
}

bool load(const Path &path, std::vector<Vertex> &verts, std::vector<TriangleI> &tris)
{
    if (path.testExtension("wo3"))
        return loadWo3(path, verts, tris);
    else if (path.testExtension("obj"))
        return loadObj(path, verts, tris);
    return false;
}

bool save(const Path &path, const std::vector<Vertex> &verts, const std::vector<TriangleI> &tris)
{
    if (path.testExtension("wo3"))
        return saveWo3(path, verts, tris);
    else if (path.testExtension("obj"))
        return saveObj(path, verts, tris);
    return false;
}

}

}
