#ifndef MESH_HPP_
#define MESH_HPP_

#include <rapidjson/document.h>
#include <memory>
#include <vector>
#include <string>

#include "Triangle.hpp"
#include "Vertex.hpp"

#include "materials/Material.hpp"

#include "math/Vec.hpp"
#include "math/Mat4f.hpp"

#include "io/JsonSerializable.hpp"

#include "Entity.hpp"

namespace Tungsten
{

class Scene;

class TriangleMesh : public Entity
{
    std::string _path;
    bool _dirty;
    bool _smoothed;

    std::vector<Vertex> _verts;
    std::vector<TriangleI> _tris;
    std::shared_ptr<Material> _material;

public:
    TriangleMesh()
    : _dirty(false),
      _smoothed(false)
    {
    }

    TriangleMesh(const TriangleMesh &o)
    : Entity(o),
      _path(o._path),
      _dirty(true),
      _smoothed(o._smoothed),
      _verts(o._verts),
      _tris(o._tris),
      _material(o._material)
    {
    }

    TriangleMesh(const rapidjson::Value &v, const Scene &scene);

    TriangleMesh(std::vector<Vertex> verts, std::vector<TriangleI> tris,
                 const std::shared_ptr<Material> &material, const std::string &name, bool smoothed)
    : Entity(name),
      _path(std::string(name).append(".wo3")),
      _dirty(true),
      _smoothed(smoothed),
      _verts(std::move(verts)),
      _tris(std::move(tris)),
      _material(material)
    {
    }

    rapidjson::Value toJson(Allocator &allocator) const;
    void saveData() const;
    void calcSmoothVertexNormals();

    virtual const TriangleMesh &asTriangleMesh() final override
    {
        return *this;
    }

    virtual void prepareForRender() final override
    {
    }

    std::shared_ptr<Material>& material()
    {
        return _material;
    }

    const std::vector<TriangleI>& tris() const
    {
        return _tris;
    }

    const std::vector<Vertex>& verts() const
    {
        return _verts;
    }

    std::vector<TriangleI>& tris()
    {
        return _tris;
    }

    std::vector<Vertex>& verts()
    {
        return _verts;
    }

    bool smoothed() const
    {
        return _smoothed;
    }

    void setSmoothed(bool v)
    {
        _smoothed = v;
    }

    bool isDirty() const
    {
        return _dirty;
    }

    void markDirty()
    {
        _dirty = true;
    }
};

}

#endif /* MESH_HPP_ */
