#include "Scene.hpp"

#include "DirectoryChange.hpp"
#include "JsonDocument.hpp"
#include "JsonObject.hpp"
#include "FileUtils.hpp"

#include "phasefunctions/PhaseFunctionFactory.hpp"

#include "integrators/path_tracer/PathTraceIntegrator.hpp"
#include "integrators/IntegratorFactory.hpp"

#include "primitives/PrimitiveFactory.hpp"
#include "primitives/Primitive.hpp"

#include "textures/TextureFactory.hpp"
#include "textures/BitmapTexture.hpp"
#include "textures/IesTexture.hpp"

#include "cameras/CameraFactory.hpp"
#include "cameras/PinholeCamera.hpp"

#include "media/MediumFactory.hpp"

#include "bsdfs/BsdfFactory.hpp"

#include "grids/GridFactory.hpp"

#include <tinyformat/tinyformat.hpp>
#include <rapidjson/document.h>
#include <functional>

namespace Tungsten {

Scene::Scene()
: _textureCache(std::make_shared<TextureCache>()),
  _camera(std::make_shared<PinholeCamera>()),
  _integrator(std::make_shared<PathTraceIntegrator>())
{
}

Scene::Scene(const Path &srcDir, std::shared_ptr<TextureCache> cache)
: _srcDir(srcDir),
  _textureCache(std::move(cache)),
  _camera(std::make_shared<PinholeCamera>()),
  _integrator(std::make_shared<PathTraceIntegrator>())
{
}

Scene::Scene(const Path &srcDir,
      std::vector<std::shared_ptr<Primitive>> primitives,
      std::vector<std::shared_ptr<Bsdf>> bsdfs,
      std::shared_ptr<TextureCache> cache,
      std::shared_ptr<Camera> camera)
: _srcDir(srcDir),
  _primitives(std::move(primitives)),
  _bsdfs(std::move(bsdfs)),
  _textureCache(std::move(cache)),
  _camera(std::move(camera)),
  _integrator(std::make_shared<PathTraceIntegrator>())
{
}

template<typename T>
std::shared_ptr<T> instantiate(JsonPtr value, const Scene &scene)
{
    auto result = StringableEnum<std::function<std::shared_ptr<T>()>>(value.getRequiredMember("type")).toEnum()();
    result->fromJson(value, scene);
    return result;
}

template<typename T>
std::shared_ptr<T> findObject(const std::vector<std::shared_ptr<T>> &list, const std::string &name, JsonPtr value)
{
    for (const std::shared_ptr<T> &t : list)
        if (t->name() == name)
            return t;
    value.parseError(tfm::format("Unable to find an object with name '%s'", name));
    return nullptr;
}

template<typename T>
std::shared_ptr<T> fetchObject(const std::vector<std::shared_ptr<T>> &list, const Scene &scene, JsonPtr value)
{
    if (value.isString()) {
        return findObject(list, value.cast<std::string>(), value);
    } else if (value.isObject()) {
        return instantiate<T>(value, scene);
    } else {
        value.parseError("Type mismatch: Expecting either an object or an object reference here");
        return nullptr;
    }
}

std::shared_ptr<Transmittance> Scene::fetchTransmittance(JsonPtr value) const
{
    return instantiate<Transmittance>(value, *this);
}

std::shared_ptr<PhaseFunction> Scene::fetchPhase(JsonPtr value) const
{
    return instantiate<PhaseFunction>(value, *this);
}

std::shared_ptr<Medium> Scene::fetchMedium(JsonPtr value) const
{
    return fetchObject(_media, *this, value);
}

std::shared_ptr<Grid> Scene::fetchGrid(JsonPtr value) const
{
    return instantiate<Grid>(value, *this);
}

std::shared_ptr<Primitive> Scene::fetchPrimitive(JsonPtr value) const
{
    return instantiate<Primitive>(value, *this);
}

std::shared_ptr<Bsdf> Scene::fetchBsdf(JsonPtr value) const
{
    using namespace std::placeholders;
    auto result = fetchObject(_bsdfs, *this, value);
    return std::move(result);
}

std::shared_ptr<Texture> Scene::fetchTexture(JsonPtr value, TexelConversion conversion) const
{
    // Note: TexelConversions are only honored by BitmapTexture.
    // This is inconsistent, but conversions do not really make sense for other textures,
    // unless the user expects e.g. a ConstantTexture with Vec3 argument to select the green
    // channel when used in a TransparencyBsdf.
    if (value.isString()) {
        return _textureCache->fetchTexture(fetchResource(value), conversion);
    } else if (value.isNumber()) {
        return std::make_shared<ConstantTexture>(value.cast<float>());
    } else if (value.isArray()) {
        return std::make_shared<ConstantTexture>(value.cast<Vec3f>());
    } else if (value.isObject()) {
        std::string type = value.castField<std::string>("type");
        if (type == "bitmap")
            return _textureCache->fetchTexture(value, conversion, this);
        else if (type == "ies")
            return _textureCache->fetchIesTexture(value, this);
        else
            return instantiate<Texture>(value, *this);
    } else {
        value.parseError("Type mismatch: Expecting a texture here");
    }
    return nullptr;
}

PathPtr Scene::fetchResource(const std::string &path) const
{
    Path key = Path(path).normalize();

    auto iter = _resources.find(key);
    if (iter == _resources.end()) {
        std::shared_ptr<Path> resource = std::make_shared<Path>(path);
        resource->freezeWorkingDirectory();

        _resources.insert(std::make_pair(key, resource));

        return std::move(resource);
    } else {
        return iter->second;
    }
}

PathPtr Scene::fetchResource(JsonPtr value) const
{
    return fetchResource(value.cast<std::string>());
}

const Primitive *Scene::findPrimitive(const std::string &name) const
{
    for (const std::shared_ptr<Primitive> &m : _primitives)
        if (m->name() == name)
            return m.get();
    return nullptr;
}

template<typename T>
bool addUnique(const std::shared_ptr<T> &o, std::vector<std::shared_ptr<T>> &list)
{
    bool retry = false;
    int dupeCount = 0;
    std::string baseName = o->name();
    if (baseName.empty())
        return true;
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

void Scene::addPrimitive(const std::shared_ptr<Primitive> &mesh)
{
    if (addUnique(mesh, _primitives)) {
        for (int i = 0; i < mesh->numBsdfs(); ++i)
            addBsdf(mesh->bsdf(i));
        if (mesh->intMedium())
            addUnique(mesh->intMedium(), _media);
        if (mesh->extMedium())
            addUnique(mesh->extMedium(), _media);
    }
}

void Scene::addBsdf(const std::shared_ptr<Bsdf> &bsdf)
{
    addUnique(bsdf, _bsdfs);
}

void Scene::merge(Scene scene)
{
    for (std::shared_ptr<Primitive> &m : scene._primitives)
        addPrimitive(m);
}

void Scene::fromJson(JsonPtr value, const Scene &scene)
{
    JsonSerializable::fromJson(value, scene);

    if (auto media = value["media"])
        for (unsigned i = 0; i < media.size(); ++i)
            _media.emplace_back(instantiate<Medium>(media[i], *this));
    if (auto bsdfs = value["bsdfs"])
        for (unsigned i = 0; i < bsdfs.size(); ++i)
            _bsdfs.emplace_back(instantiate<Bsdf>(bsdfs[i], *this));
    if (auto primitives = value["primitives"])
        for (unsigned i = 0; i < primitives.size(); ++i)
            _primitives.emplace_back(instantiate<Primitive>(primitives[i], *this));

    if (auto camera     = value["camera"    ]) _camera     = instantiate<Camera>(camera, *this);
    if (auto integrator = value["integrator"]) _integrator = instantiate<Integrator>(integrator, *this);
    if (auto renderer   = value["renderer"  ]) _rendererSettings.fromJson(renderer, *this);
}

rapidjson::Value Scene::toJson(Allocator &allocator) const
{

    rapidjson::Value media(rapidjson::kArrayType);
    for (const std::shared_ptr<Medium> &b : _media)
        media.PushBack(b->toJson(allocator), allocator);

    rapidjson::Value bsdfs(rapidjson::kArrayType);
    for (const std::shared_ptr<Bsdf> &b : _bsdfs)
        bsdfs.PushBack(b->toJson(allocator), allocator);

    rapidjson::Value primitives(rapidjson::kArrayType);
    for (const std::shared_ptr<Primitive> &t : _primitives)
        if (!_helperPrimitives.count(t.get()))
            primitives.PushBack(t->toJson(allocator), allocator);

    return JsonObject{JsonSerializable::toJson(allocator), allocator,
        "media", std::move(media),
        "bsdfs", std::move(bsdfs),
        "primitives", std::move(primitives),
        "camera", *_camera,
        "integrator", *_integrator,
        "renderer", _rendererSettings
    };
}

void Scene::loadResources()
{
    for (const std::shared_ptr<Medium> &b : _media)
        b->loadResources();
    for (const std::shared_ptr<Bsdf> &b : _bsdfs)
        b->loadResources();
    for (const std::shared_ptr<Primitive> &t : _primitives)
        t->loadResources();

    _camera->loadResources();
    _integrator->loadResources();
    _rendererSettings.loadResources();

    _textureCache->loadResources();

    for (size_t i = 0; i < _primitives.size(); ++i) {
        auto helperPrimitives = _primitives[i]->createHelperPrimitives();
        if (!helperPrimitives.empty()) {
            _primitives.reserve(_primitives.size() + helperPrimitives.size());
            for (size_t t = 0; t < helperPrimitives.size(); ++t) {
                _helperPrimitives.insert(helperPrimitives[t].get());
                _primitives.emplace_back(std::move(helperPrimitives[t]));
            }
        }
    }
}

void Scene::saveResources()
{
    for (const std::shared_ptr<Medium> &b : _media)
        b->saveResources();
    for (const std::shared_ptr<Bsdf> &b : _bsdfs)
        b->saveResources();
    for (const std::shared_ptr<Primitive> &t : _primitives)
        t->saveResources();

    _camera->saveResources();
    _integrator->saveResources();
    _rendererSettings.saveResources();
}

template<typename T>
void deleteObjects(std::vector<std::shared_ptr<T>> &dst, const std::unordered_set<T *> &objects)
{
    std::vector<std::shared_ptr<T>> newObjects;
    newObjects.reserve(dst.size());

    for (std::shared_ptr<T> &m : dst)
        if (!objects.count(m.get()))
            newObjects.push_back(m);

    dst = std::move(newObjects);
}

template<typename T>
void pruneObjects(std::vector<std::shared_ptr<T>> &dst)
{
    std::vector<std::shared_ptr<T>> newObjects;
    newObjects.reserve(dst.size());

    for (std::shared_ptr<T> &m : dst)
        if (m.use_count() > 1)
            newObjects.push_back(m);

    dst = std::move(newObjects);
}

void Scene::deletePrimitives(const std::unordered_set<Primitive *> &primitives)
{
    deleteObjects(_primitives, primitives);
}

void Scene::deleteBsdfs(const std::unordered_set<Bsdf *> &bsdfs)
{
    deleteObjects(_bsdfs, bsdfs);
}

void Scene::deleteMedia(const std::unordered_set<Medium *> &media)
{
    deleteObjects(_media, media);
}

void Scene::pruneBsdfs()
{
    pruneObjects(_bsdfs);
}

void Scene::pruneMedia()
{
    pruneObjects(_media);
}

TraceableScene *Scene::makeTraceable(uint32 seed)
{
    return new TraceableScene(*_camera, *_integrator, _primitives, _bsdfs, _media, _rendererSettings, seed);
}

Scene *Scene::load(const Path &path, std::shared_ptr<TextureCache> cache, const Path *inputDirectory)
{
    JsonDocument document(path);

    DirectoryChange context(inputDirectory ? *inputDirectory : path.parent());
    if (!cache)
        cache = std::make_shared<TextureCache>();

    Scene *scene = new Scene(path.parent(), std::move(cache));
    scene->fromJson(document, *scene);
    scene->setPath(path);

    return scene;
}

void Scene::save(const Path &path, const Scene &scene)
{
    rapidjson::Document document;
    document.SetObject();

    *(static_cast<rapidjson::Value *>(&document)) = scene.toJson(document.GetAllocator());

    FileUtils::writeJson(document, path);
}

}
