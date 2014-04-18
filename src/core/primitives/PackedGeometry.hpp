#ifndef PACKEDGEOMETRY_HPP_
#define PACKEDGEOMETRY_HPP_

#include <rapidjson/document.h>
#include <embree/include/embree.h>
#include <vector>
#include <memory>

#include "Triangle.hpp"
#include "Mesh.hpp"

#include "materials/Material.hpp"

#include "bsdfs/Bsdf.hpp"

namespace Tungsten
{

class PackedGeometry
{
    std::vector<Material> _materials;
    std::vector<Triangle> _tris;

    embree::RTCGeometry *_geom;
    embree::RTCIntersector1 *_intersector;

public:
    enum TriangleFlags
    {
        FlatFlag       = (1 << 0),
    };

    PackedGeometry(const PackedGeometry &) = delete;
    PackedGeometry(PackedGeometry&&) = default;

    PackedGeometry(const std::vector<std::shared_ptr<TriangleMesh>> &meshes)
    {
        uint64 triangleCount = 0, vertexCount = 0;

        for (const std::shared_ptr<TriangleMesh> &m : meshes) {
            triangleCount += m-> tris().size();
            vertexCount   += m->verts().size();
        }

        _tris.reserve(triangleCount);
        _materials.reserve(meshes.size());

        _geom = embree::rtcNewTriangleMesh(triangleCount, vertexCount, "bvh2");
        embree::RTCVertex   *vs = embree::rtcMapPositionBuffer(_geom);
        embree::RTCTriangle *ts = embree::rtcMapTriangleBuffer(_geom);

        uint64 indexBase = 0, triangleBase = 0;
        for (const std::shared_ptr<TriangleMesh> &m : meshes) {
            const std::vector<Vertex> &verts = m->verts();
            for (const TriangleI &t : m->tris()) {
                _tris.emplace_back(verts[t.v0], verts[t.v1], verts[t.v2], _materials.size(), 0, 1);

                int32 triangleFlags = 0;
                if (!m->smoothed())
                    triangleFlags |= FlatFlag;

                ts[triangleBase] = embree::RTCTriangle(t.v0 + indexBase, t.v1 + indexBase, t.v2 + indexBase, triangleBase, triangleFlags);
                triangleBase++;
            }

            Mat4f tform = m->transform();
            for (const Vertex &v : verts) {
                Vec3f p = tform*v.pos();
                vs[indexBase++] = embree::RTCVertex(p.x(), p.y(), p.z());
            }

            _materials.push_back(*m->material());
        }

        embree::rtcUnmapPositionBuffer(_geom);
        embree::rtcUnmapTriangleBuffer(_geom);

        embree::rtcBuildAccel(_geom, "objectsplit");
        _intersector = embree::rtcQueryIntersector1(_geom, "fast.moeller");
    }

    const embree::RTCGeometry *geom() const
    {
        return _geom;
    }

    const embree::RTCIntersector1 *intersector() const
    {
        return _intersector;
    }

    const std::vector<Triangle> &tris() const
    {
        return _tris;
    }

    const std::vector<Material> &materials() const
    {
        return _materials;
    }
};

}


#endif /* PACKEDGEOMETRY_HPP_ */
