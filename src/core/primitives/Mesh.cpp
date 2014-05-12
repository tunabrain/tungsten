#include "Mesh.hpp"

#include "io/MeshInputOutput.hpp"
#include "io/JsonUtils.hpp"
#include "io/Scene.hpp"

#include <unordered_map>

namespace Tungsten
{

void TriangleMesh::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    Primitive::fromJson(v, scene);

    _dirty = false;
    _path = JsonUtils::as<std::string>(v, "file");
    JsonUtils::fromJson(v, "smooth", _smoothed);

    MeshInputOutput::load(_path, _verts, _tris);
}

rapidjson::Value TriangleMesh::toJson(Allocator &allocator) const
{
    rapidjson::Value v = Primitive::toJson(allocator);
    v.AddMember("type", "mesh", allocator);
    v.AddMember("file", _path.c_str(), allocator);
    v.AddMember("smooth", _smoothed, allocator);
    return std::move(v);
}

void TriangleMesh::saveData() const
{
    if (_dirty)
        MeshInputOutput::save(_path, _verts, _tris);
}

void TriangleMesh::saveAsObj(std::ostream &out) const
{
    for (const Vertex &v : _verts)
        out << tfm::format("v %f %f %f\n", v.pos().x(), v.pos().y(), v.pos().z());
    for (const Vertex &v : _verts)
        out << tfm::format("vn %f %f %f\n", v.normal().x(), v.normal().y(), v.normal().z());
    for (const Vertex &v : _verts)
        out << tfm::format("vt %f %f\n", v.uv().x(), v.uv().y());
    for (const TriangleI &t : _tris)
        out << tfm::format("f %d/%d/%d %d/%d/%d %d/%d/%d\n",
                t.v0 + 1, t.v0 + 1, t.v0 + 1,
                t.v1 + 1, t.v1 + 1, t.v1 + 1,
                t.v2 + 1, t.v2 + 1, t.v2 + 1);
}

void TriangleMesh::calcSmoothVertexNormals()
{
    static constexpr float SplitLimit = std::cos(PI*0.25f);
    //static constexpr float SplitLimit = -1.0f;

    std::vector<Vec3f> geometricN(_verts.size(), Vec3f(0.0f));
    std::unordered_multimap<Vec3f, uint32> posToVert;

    for (uint32 i = 0; i < _verts.size(); ++i) {
        _verts[i].normal() = Vec3f(0.0f);
        posToVert.insert(std::make_pair(_verts[i].pos(), i));
    }

    for (TriangleI &t : _tris) {
        const Vec3f &p0 = _verts[t.v0].pos();
        const Vec3f &p1 = _verts[t.v1].pos();
        const Vec3f &p2 = _verts[t.v2].pos();
        Vec3f normal = (p1 - p0).cross(p2 - p0).normalized();

        for (int i = 0; i < 3; ++i) {
            Vec3f &n = geometricN[t.vs[i]];
            if (n == 0.0f) {
                n = normal;
            } else if (n.dot(normal) < SplitLimit) {
                _verts.push_back(_verts[t.vs[i]]);
                geometricN.push_back(normal);
                t.vs[i] = _verts.size() - 1;
            }
        }
    }

    for (TriangleI &t : _tris) {
        const Vec3f &p0 = _verts[t.v0].pos();
        const Vec3f &p1 = _verts[t.v1].pos();
        const Vec3f &p2 = _verts[t.v2].pos();
        Vec3f normal = (p1 - p0).cross(p2 - p0);
        Vec3f nN = normal.normalized();

        for (int i = 0; i < 3; ++i) {
            auto iters = posToVert.equal_range(_verts[t.vs[i]].pos());

            for (auto t = iters.first; t != iters.second; ++t)
                if (geometricN[t->second].dot(nN) >= SplitLimit)
                    _verts[t->second].normal() += normal;
        }
    }

    for (Vertex &v : _verts)
        v.normal().normalize();
}

void TriangleMesh::computeBounds()
{
    Box3f box;
    for (Vertex &v : _verts)
        box.grow(_transform*v.pos());
    _bounds = box;
}


}
