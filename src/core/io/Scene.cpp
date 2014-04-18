#include <functional>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/filewritestream.h>

#include "JsonUtils.hpp"
#include "FileUtils.hpp"
#include "Scene.hpp"

#include "primitives/Mesh.hpp"

#include "materials/Material.hpp"

#include "lights/DirectionalLight.hpp"
#include "lights/EnvironmentLight.hpp"
#include "lights/SphereLight.hpp"
#include "lights/PointLight.hpp"
#include "lights/QuadLight.hpp"
#include "lights/ConeLight.hpp"

#include "bsdfs/DielectricBsdf.hpp"
#include "bsdfs/LambertBsdf.hpp"
#include "bsdfs/PhongBsdf.hpp"
#include "bsdfs/MixedBsdf.hpp"
#include "bsdfs/Bsdf.hpp"

#include "Debug.hpp"

namespace Tungsten
{

std::shared_ptr<Material> Scene::instantiateMaterial(std::string type, const rapidjson::Value &value) const
{
    if (type == "constant") {
        return std::shared_ptr<Material>(new Material(value, *this));
    } else {
        FAIL("Unkown material type: '%s'", type.c_str());
        return nullptr;
    }
}

std::shared_ptr<Light> Scene::instantiateLight(std::string type, const rapidjson::Value &value) const
{
    if (type == "quad") {
        return std::shared_ptr<Light>(new QuadLight(value, *this));
    } else if (type == "point") {
        return std::shared_ptr<Light>(new PointLight(value, *this));
    } else if (type == "directional") {
        return std::shared_ptr<Light>(new DirectionalLight(value, *this));
    } else if (type == "environment") {
        return std::shared_ptr<Light>(new EnvironmentLight(value, *this));
    } else if (type == "cone") {
        return std::shared_ptr<Light>(new ConeLight(value, *this));
    } else if (type == "sphere") {
        return std::shared_ptr<Light>(new SphereLight(value, *this));
    } else {
        FAIL("Unkown light type: '%s'", type.c_str());
        return nullptr;
    }
}

std::shared_ptr<Bsdf> Scene::instantiateBsdf(std::string type, const rapidjson::Value &value) const
{
    if (type == "lambert") {
        return std::shared_ptr<Bsdf>(new LambertBsdf(value, *this));
    } else if (type == "phong") {
        return std::shared_ptr<Bsdf>(new PhongBsdf(value, *this));
    } else if (type == "mixed") {
        return std::shared_ptr<Bsdf>(new MixedBsdf(value, *this));
    } else if (type == "dielectric") {
        return std::shared_ptr<Bsdf>(new DielectricBsdf(value, *this));
    } else {
        FAIL("Unkown bsdf type: '%s'", type.c_str());
        return nullptr;
    }
}

std::shared_ptr<TriangleMesh> Scene::instantiatePrimitive(std::string type, const rapidjson::Value &value) const
{
    if (type == "mesh") {
        return std::shared_ptr<TriangleMesh>(new TriangleMesh(value, *this));
    } else {
        FAIL("Unknown primitive type: '%s'", type.c_str());
        return nullptr;
    }
}

template<typename Instantiator, typename Element>
void Scene::loadObjectList(const rapidjson::Value &container, Instantiator instantiator, std::vector<std::shared_ptr<Element>> &result)
{
    for (unsigned i = 0; i < container.Size(); ++i) {
        if (container[i].IsObject())
            result.push_back(instantiator(JsonUtils::fromJsonMember<std::string>(container[i], "type"), container[i]));
        else
            LOG("Scene", WARN, "Don't know what to do with non-object");
    }
}

template<typename T>
std::shared_ptr<T> Scene::findObject(const std::vector<std::shared_ptr<T>> &list, std::string name) const
{
    for (const std::shared_ptr<T> &t : list)
        if (t->name() == name)
            return t;
    return nullptr;
}

template<typename T, typename Instantiator>
std::shared_ptr<T> Scene::fetchObject(const std::vector<std::shared_ptr<T>> &list, const rapidjson::Value &v, Instantiator instantiator) const
{
    if (v.IsString()) {
        return findObject(list, v.GetString());
    } else if (v.IsObject()) {
        return instantiator(JsonUtils::fromJsonMember<std::string>(v, "type"), v);
    } else {
        return nullptr;
    }
}

std::shared_ptr<Bsdf> Scene::fetchBsdf(const rapidjson::Value &v) const
{
    using namespace std::placeholders;
    return fetchObject(_bsdfs, v, std::bind(&Scene::instantiateBsdf, this, _1, _2));
}

std::shared_ptr<Material> Scene::fetchMaterial(const rapidjson::Value &v) const
{
    using namespace std::placeholders;
    return fetchObject(_materials, v, std::bind(&Scene::instantiateMaterial, this, _1, _2));
}

std::shared_ptr<TextureRgba> Scene::fetchColorMap(const std::string &path) const
{
    if (_colorMaps.count(path))
        return _colorMaps[path];

    std::shared_ptr<TextureRgba> tex(new TextureRgba(path));
    _colorMaps[path] = tex;
    return tex;
}

std::shared_ptr<TextureA> Scene::fetchScalarMap(const std::string &path) const
{
    if (_scalarMaps.count(path))
        return _scalarMaps[path];

    std::shared_ptr<TextureA> tex(new TextureA(path));
    _scalarMaps[path] = tex;
    return tex;
}

template<typename T>
bool Scene::addUnique(const std::shared_ptr<T> &o, std::vector<std::shared_ptr<T>> &list)
{
    bool retry = false;
    int dupeCount = 0;
    std::string baseName = o->name();
    for (int i = 1; !baseName.empty() && isdigit(baseName.back()); i *= 10, baseName.pop_back())
        dupeCount += i*(baseName.back() - '0');
    std::string newName = o->name();
    do {
        retry = false;
        for (const std::shared_ptr<T> &m : list) {
            if (m.get() == o.get())
                return false;
            if (m->name() == newName) {
                newName = tfm::format("%s%d", baseName, ++dupeCount);
                retry = true;
                break;
            }
        }
    } while (retry);

    o->setName(newName);
    list.push_back(o);

    return true;
}

template<typename T>
void Scene::addTexture(std::shared_ptr<T> &t, std::map<std::string, std::shared_ptr<T>> &maps)
{
    if (!t)
        return;
    if (!maps.count(t->path()))
        return;

    if (maps[t->path()]->fullPath() == t->fullPath()) {
        t = maps[t->path()];
    } else {
        int i = 1;
        std::string newPath = t->path();
        while (maps.count(newPath))
            newPath = tfm::format("%s%d.%s", FileUtils::stripExt(t->path()), i++, FileUtils::extractExt(t->path()));
        t->setPath(newPath);
    }
}

void Scene::addMesh(const std::shared_ptr<TriangleMesh> &mesh)
{
    if (addUnique(mesh, _primitives))
        addMaterial(mesh->material());
    _entities.clear();
}

void Scene::addMaterial(const std::shared_ptr<Material> &material)
{
    if (addUnique(material, _materials)) {
        addBsdf(material->bsdf());
        addTexture(material->alpha(), _scalarMaps);
        addTexture(material->bump(), _scalarMaps);
        addTexture(material->color(), _colorMaps);
    }
}

void Scene::addBsdf(const std::shared_ptr<Bsdf> &bsdf)
{
    addUnique(bsdf, _bsdfs);
}

void Scene::addLight(const std::shared_ptr<Light> &light)
{
    addUnique(light, _lights);
    _entities.clear();
}

void Scene::merge(Scene scene)
{
    for (std::shared_ptr<TriangleMesh> &m : scene._primitives)
        addMesh(m);
    for (std::shared_ptr<Light> &l : scene._lights)
        addLight(l);
}

Scene::Scene(const std::string &srcDir, const rapidjson::Value &v)
: JsonSerializable(v), _srcDir(srcDir)
{
    using namespace std::placeholders;

    const rapidjson::Value::Member *primitives = v.FindMember("primitives");
    const rapidjson::Value::Member *materials  = v.FindMember("materials");
    const rapidjson::Value::Member *lights     = v.FindMember("lights");
    const rapidjson::Value::Member *bsdfs      = v.FindMember("bsdfs");
    const rapidjson::Value::Member *camera     = v.FindMember("camera");

    SOFT_ASSERT(primitives != nullptr, "Scene file must contain 'primitives' array");
    SOFT_ASSERT(materials  != nullptr, "Scene file must contain 'materials' array");
    SOFT_ASSERT(camera     != nullptr, "Scene file must contain 'camera' object");

    if (bsdfs)
        loadObjectList(bsdfs->value, std::bind(&Scene::instantiateBsdf, this, _1, _2), _bsdfs);

    loadObjectList(materials ->value, std::bind(&Scene::instantiateMaterial,  this, _1, _2),  _materials);
    loadObjectList(primitives->value, std::bind(&Scene::instantiatePrimitive, this, _1, _2), _primitives);

    if (lights)
        loadObjectList(lights->value, std::bind(&Scene::instantiateLight, this, _1, _2), _lights);

    _camera = std::shared_ptr<Camera>(new Camera(camera->value, *this));
}

Scene::Scene(const std::string &srcDir,
      std::vector<std::shared_ptr<TriangleMesh>> primitives,
      std::vector<std::shared_ptr<Material>> materials,
      std::vector<std::shared_ptr<Light>> lights,
      std::vector<std::shared_ptr<Bsdf>> bsdfs,
      std::shared_ptr<Camera> camera)
: _srcDir(srcDir),
  _primitives(std::move(primitives)),
  _materials(std::move(materials)),
  _lights(std::move(lights)),
  _bsdfs(std::move(bsdfs)),
  _camera(camera)
{
    for (const std::shared_ptr<Material> &m : _materials) {
        if (m->color())
            _colorMaps.insert(std::make_pair(m->color()->path(), m->color()));
        if (m->alpha())
            _scalarMaps.insert(std::make_pair(m->alpha()->path(), m->alpha()));
        if (m->bump())
            _scalarMaps.insert(std::make_pair(m->bump()->path(), m->bump()));
    }
}

rapidjson::Value Scene::toJson(Allocator &allocator) const
{
    rapidjson::Value v = JsonSerializable::toJson(allocator);

    rapidjson::Value primitives(rapidjson::kArrayType);
    for (const std::shared_ptr<TriangleMesh> &t : _primitives)
        primitives.PushBack(t->toJson(allocator), allocator);

    rapidjson::Value materials(rapidjson::kArrayType);
    for (const std::shared_ptr<Material> &m : _materials)
        materials.PushBack(m->toJson(allocator), allocator);

    rapidjson::Value lights(rapidjson::kArrayType);
    for (const std::shared_ptr<Light> &e : _lights)
        lights.PushBack(e->toJson(allocator), allocator);

    rapidjson::Value bsdfs(rapidjson::kArrayType);
    for (const std::shared_ptr<Bsdf> &b : _bsdfs)
        bsdfs.PushBack(b->toJson(allocator), allocator);

    v.AddMember("bsdfs", bsdfs, allocator);
    v.AddMember("materials", materials, allocator);
    v.AddMember("lights", lights, allocator);
    v.AddMember("primitives", primitives, allocator);
    v.AddMember("camera", _camera->toJson(allocator), allocator);

    return std::move(v);
}

void Scene::saveData(const std::string &dst) const
{
    std::string previousDir = FileUtils::getCurrentDir();
    FileUtils::changeCurrentDir(dst);

    for (const std::shared_ptr<TriangleMesh> &m : _primitives)
        m->saveData();

    for (const auto &p : _colorMaps)
        p.second->saveData();
    for (const auto &p : _scalarMaps)
        p.second->saveData();

    FileUtils::changeCurrentDir(previousDir);
}

void Scene::deleteEntities(const std::unordered_set<Entity *> &entities)
{
    std::vector<std::shared_ptr<TriangleMesh>> newPrims;
    std::vector<std::shared_ptr<Light>> newLights;
    newPrims.reserve(_primitives.size());
    newLights.reserve(_lights.size());

    for (std::shared_ptr<TriangleMesh> &m : _primitives)
        if (!entities.count(m.get()))
            newPrims.push_back(m);
    for (std::shared_ptr<Light> &l : _lights)
        if (!entities.count(l.get()))
            newLights.push_back(l);

    _lights = std::move(newLights);
    _primitives = std::move(newPrims);
    _entities.clear();
}

PackedGeometry Scene::flatten() const
{
    return std::move(PackedGeometry(_primitives));
}

std::vector<Entity *> &Scene::entities()
{
    if (_entities.empty() && (!_primitives.empty() || !_lights.empty())) {
        for (const std::shared_ptr<TriangleMesh> &m : _primitives)
            _entities.emplace_back(m.get());
        for (const std::shared_ptr<Light> &l : _lights)
            _entities.emplace_back(l.get());
    }

    return _entities;
}

Scene *Scene::load(const std::string &path)
{
    std::string json = FileUtils::loadText(path.c_str());

    rapidjson::Document document;
    document.Parse<0>(json.c_str());
    if (document.HasParseError()) {
        LOG("Scene::load", WARN, "JSON parse error: %s", document.GetParseError());
        return nullptr;
    }

    std::string previousDir = FileUtils::getCurrentDir();
    FileUtils::changeCurrentDir(FileUtils::extractDir(std::string(path)));

    Scene *scene = new Scene(FileUtils::extractDir(path), document);

    FileUtils::changeCurrentDir(previousDir);

    scene->setPath(path);

    return scene;
}

void Scene::save(const std::string &path, const Scene &scene, bool includeData)
{
    rapidjson::Document document;
    document.SetObject();

    *(static_cast<rapidjson::Value *>(&document)) = scene.toJson(document.GetAllocator());

    char buffer[4*1024];
    FILE *fp = fopen(path.c_str(), "wb");
    rapidjson::FileWriteStream out(fp, buffer, sizeof(buffer));

    rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(out);
    document.Accept(writer);
    fclose(fp);

    if (includeData)
        scene.saveData(FileUtils::extractDir(path));
}

}
