#ifndef SCENE_HPP_
#define SCENE_HPP_

#include <rapidjson/document.h>
#include <unordered_set>
#include <memory>
#include <vector>
#include <map>

#include "JsonSerializable.hpp"

#include "primitives/PackedGeometry.hpp"
#include "primitives/Mesh.hpp"

#include "materials/Material.hpp"

#include "lights/Light.hpp"

#include "bsdfs/Bsdf.hpp"

#include "Camera.hpp"

namespace Tungsten
{

class Scene : public JsonSerializable
{
    std::string _srcDir;

    std::vector<std::shared_ptr<TriangleMesh>> _primitives;
    std::vector<std::shared_ptr<Material>> _materials;
    std::vector<Entity *> _entities;
    std::vector<std::shared_ptr<Light>> _lights;
    std::vector<std::shared_ptr<Bsdf>> _bsdfs;
    mutable std::map<std::string, std::shared_ptr<TextureRgba>> _colorMaps;
    mutable std::map<std::string, std::shared_ptr<TextureA>> _scalarMaps;
    std::shared_ptr<Camera> _camera;

    std::string _path;

    std::shared_ptr<Material>     instantiateMaterial (std::string type, const rapidjson::Value &value) const;
    std::shared_ptr<Light>        instantiateLight    (std::string type, const rapidjson::Value &value) const;
    std::shared_ptr<Bsdf>         instantiateBsdf     (std::string type, const rapidjson::Value &value) const;
    std::shared_ptr<TriangleMesh> instantiatePrimitive(std::string type, const rapidjson::Value &value) const;

    template<typename Instantiator, typename Element>
    void loadObjectList(const rapidjson::Value &container, Instantiator instantiator, std::vector<std::shared_ptr<Element>> &result);

    template<typename T>
    std::shared_ptr<T> findObject(const std::vector<std::shared_ptr<T>> &list, std::string name) const;

    template<typename T, typename Instantiator>
    std::shared_ptr<T> fetchObject(const std::vector<std::shared_ptr<T>> &list, const rapidjson::Value &v, Instantiator instantiator) const;

    template<typename T>
    bool addUnique(const std::shared_ptr<T> &o, std::vector<std::shared_ptr<T>> &list);

    template<typename T>
    void addTexture(std::shared_ptr<T> &t, std::map<std::string, std::shared_ptr<T>> &maps);

public:
    Scene() = default;

    Scene(const std::string &srcDir, const rapidjson::Value &v);

    Scene(const std::string &srcDir,
          std::vector<std::shared_ptr<TriangleMesh>> primitives,
          std::vector<std::shared_ptr<Material>> materials,
          std::vector<std::shared_ptr<Light>> lights,
          std::vector<std::shared_ptr<Bsdf>> bsdfs,
          std::shared_ptr<Camera> camera);

    virtual rapidjson::Value toJson(Allocator &allocator) const;
    void saveData(const std::string &dst) const;

    std::shared_ptr<Bsdf> fetchBsdf(const rapidjson::Value &v) const;
    std::shared_ptr<Material> fetchMaterial(const rapidjson::Value &v) const;
    std::shared_ptr<TextureRgba> fetchColorMap(const std::string &path) const;
    std::shared_ptr<TextureA> fetchScalarMap(const std::string &path) const;

    void addMesh(const std::shared_ptr<TriangleMesh> &mesh);
    void addMaterial(const std::shared_ptr<Material> &material);
    void addBsdf(const std::shared_ptr<Bsdf> &bsdf);
    void addLight(const std::shared_ptr<Light> &light);
    void merge(Scene scene);

    void deleteEntities(const std::unordered_set<Entity *> &entities);

    PackedGeometry flatten() const;

    std::vector<Entity *> &entities();

    std::vector<std::shared_ptr<TriangleMesh>> &primitives()
    {
        return _primitives;
    }

    std::vector<std::shared_ptr<Light>> &lights()
    {
        return _lights;
    }

    std::shared_ptr<Camera> camera()
    {
        return _camera;
    }

    void setPath(const std::string &p)
    {
        _path = p;
    }

    const std::string path() const
    {
        return _path;
    }

    void setCamera(Camera *cam)
    {
        _camera.reset(cam);
    }

    static Scene *load(const std::string &path);
    static void save(const std::string &path, const Scene &scene, bool includeData);
};

}


#endif /* SCENE_HPP_ */
