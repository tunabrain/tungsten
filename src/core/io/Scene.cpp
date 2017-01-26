#include "Scene.hpp"
#include "DirectoryChange.hpp"
#include "JsonUtils.hpp"
#include "FileUtils.hpp"

#include "phasefunctions/HenyeyGreensteinPhaseFunction.hpp"
#include "phasefunctions/IsotropicPhaseFunction.hpp"
#include "phasefunctions/RayleighPhaseFunction.hpp"

#include "integrators/bidirectional_path_tracer/BidirectionalPathTraceIntegrator.hpp"
#include "integrators/progressive_photon_map/ProgressivePhotonMapIntegrator.hpp"
#include "integrators/light_tracer/LightTraceIntegrator.hpp"
#include "integrators/kelemen_mlt/KelemenMltIntegrator.hpp"
#include "integrators/path_tracer/PathTraceIntegrator.hpp"
#include "integrators/photon_map/PhotonMapIntegrator.hpp"

#include "primitives/mc-loader/TraceableMinecraftMap.hpp"
#include "primitives/InfiniteSphereCap.hpp"
#include "primitives/InfiniteSphere.hpp"
#include "primitives/TriangleMesh.hpp"
#include "primitives/Skydome.hpp"
#include "primitives/Sphere.hpp"
#include "primitives/Curves.hpp"
#include "primitives/Point.hpp"
#include "primitives/Cube.hpp"
#include "primitives/Quad.hpp"
#include "primitives/Disk.hpp"

#include "materials/ConstantTexture.hpp"
#include "materials/CheckerTexture.hpp"
#include "materials/BladeTexture.hpp"
#include "materials/DiskTexture.hpp"
#include "materials/IesTexture.hpp"

#include "cameras/EquirectangularCamera.hpp"
#include "cameras/ThinlensCamera.hpp"
#include "cameras/CubemapCamera.hpp"
#include "cameras/PinholeCamera.hpp"

#include "media/HomogeneousMedium.hpp"
#include "media/AtmosphericMedium.hpp"
#include "media/ExponentialMedium.hpp"
#include "media/VoxelMedium.hpp"

#include "bcsdfs/LambertianFiberBcsdf.hpp"
#include "bcsdfs/RoughWireBcsdf.hpp"
#include "bcsdfs/HairBcsdf.hpp"

#include "bsdfs/RoughDielectricBsdf.hpp"
#include "bsdfs/RoughConductorBsdf.hpp"
#include "bsdfs/RoughPlasticBsdf.hpp"
#include "bsdfs/TransparencyBsdf.hpp"
#include "bsdfs/DielectricBsdf.hpp"
#include "bsdfs/SmoothCoatBsdf.hpp"
#include "bsdfs/RoughCoatBsdf.hpp"
#include "bsdfs/ConductorBsdf.hpp"
#include "bsdfs/OrenNayarBsdf.hpp"
#include "bsdfs/ThinSheetBsdf.hpp"
#include "bsdfs/ForwardBsdf.hpp"
#include "bsdfs/LambertBsdf.hpp"
#include "bsdfs/PlasticBsdf.hpp"
#include "bsdfs/MirrorBsdf.hpp"
#include "bsdfs/ErrorBsdf.hpp"
#include "bsdfs/PhongBsdf.hpp"
#include "bsdfs/MixedBsdf.hpp"
#include "bsdfs/NullBsdf.hpp"
#include "bsdfs/Bsdf.hpp"

#include "grids/VdbGrid.hpp"

#include "JsonDocument.hpp"
#include "JsonObject.hpp"

#include "Debug.hpp"

#include <rapidjson/document.h>
#include <functional>

namespace Tungsten {

Scene::Scene()
: _errorBsdf(std::make_shared<ErrorBsdf>()),
  _errorTexture(std::make_shared<ConstantTexture>(Vec3f(1.0f, 0.0f, 0.0f))),
  _textureCache(std::make_shared<TextureCache>()),
  _camera(std::make_shared<PinholeCamera>()),
  _integrator(std::make_shared<PathTraceIntegrator>())
{
}

Scene::Scene(const Path &srcDir, std::shared_ptr<TextureCache> cache)
: _srcDir(srcDir),
  _errorBsdf(std::make_shared<ErrorBsdf>()),
  _errorTexture(std::make_shared<ConstantTexture>(Vec3f(1.0f, 0.0f, 0.0f))),
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
  _errorBsdf(std::make_shared<ErrorBsdf>()),
  _errorTexture(std::make_shared<ConstantTexture>(Vec3f(1.0f, 0.0f, 0.0f))),
  _textureCache(std::move(cache)),
  _camera(std::move(camera)),
  _integrator(std::make_shared<PathTraceIntegrator>())
{
}

std::shared_ptr<PhaseFunction> Scene::instantiatePhase(JsonValue value) const
{
    std::shared_ptr<PhaseFunction> result;

    std::string type = value.castField<std::string>("type");
    if (type == "isotropic")
        result = std::make_shared<IsotropicPhaseFunction>();
    else if (type == "henyey_greenstein")
        result = std::make_shared<HenyeyGreensteinPhaseFunction>();
    else if (type == "rayleigh")
        result = std::make_shared<RayleighPhaseFunction>();
    else {
        DBG("Unkown phase function type: '%s'", type.c_str());
        return nullptr;
    }
    result->fromJson(value, *this);
    return result;
}

std::shared_ptr<Medium> Scene::instantiateMedium(JsonValue value) const
{
    std::shared_ptr<Medium> result;

    std::string type = value.castField<std::string>("type");
    if (type == "homogeneous")
        result = std::make_shared<HomogeneousMedium>();
    else if (type == "atmosphere")
        result = std::make_shared<AtmosphericMedium>();
    else if (type == "exponential")
        result = std::make_shared<ExponentialMedium>();
    else if (type == "voxel")
        result = std::make_shared<VoxelMedium>();
    else {
        DBG("Unkown medium type: '%s'", type.c_str());
        return nullptr;
    }
    result->fromJson(value, *this);
    return result;
}

std::shared_ptr<Grid> Scene::instantiateGrid(JsonValue value) const
{
    std::shared_ptr<Grid> result;

    std::string type = value.castField<std::string>("type");
#if OPENVDB_AVAILABLE
    if (type == "vdb")
        result = std::make_shared<VdbGrid>();
    else
#endif
    {
        DBG("Unkown grid type: '%s'", type.c_str());
        return nullptr;
    }
    result->fromJson(value, *this);
    return result;
}

std::shared_ptr<Bsdf> Scene::instantiateBsdf(JsonValue value) const
{
    std::shared_ptr<Bsdf> result;

    std::string type = value.castField<std::string>("type");
    if (type == "lambert")
        result = std::make_shared<LambertBsdf>();
    else if (type == "phong")
        result = std::make_shared<PhongBsdf>();
    else if (type == "mixed")
        result = std::make_shared<MixedBsdf>();
    else if (type == "dielectric")
        result = std::make_shared<DielectricBsdf>();
    else if (type == "conductor")
        result = std::make_shared<ConductorBsdf>();
    else if (type == "mirror")
        result = std::make_shared<MirrorBsdf>();
    else if (type == "rough_conductor")
        result = std::make_shared<RoughConductorBsdf>();
    else if (type == "rough_dielectric")
        result = std::make_shared<RoughDielectricBsdf>();
    else if (type == "smooth_coat")
        result = std::make_shared<SmoothCoatBsdf>();
    else if (type == "null")
        result = std::make_shared<NullBsdf>();
    else if (type == "forward")
        result = std::make_shared<ForwardBsdf>();
    else if (type == "thinsheet")
        result = std::make_shared<ThinSheetBsdf>();
    else if (type == "oren_nayar")
        result = std::make_shared<OrenNayarBsdf>();
    else if (type == "plastic")
        result = std::make_shared<PlasticBsdf>();
    else if (type == "rough_plastic")
        result = std::make_shared<RoughPlasticBsdf>();
    else if (type == "rough_coat")
        result = std::make_shared<RoughCoatBsdf>();
    else if (type == "transparency")
        result = std::make_shared<TransparencyBsdf>();
    else if (type == "lambertian_fiber")
        result = std::make_shared<LambertianFiberBcsdf>();
    else if (type == "rough_wire")
        result = std::make_shared<RoughWireBcsdf>();
    else if (type == "hair")
        result = std::make_shared<HairBcsdf>();
    else {
        DBG("Unkown bsdf type: '%s'", type.c_str());
        return nullptr;
    }
    result->fromJson(value, *this);
    return result;
}

std::shared_ptr<Primitive> Scene::instantiatePrimitive(JsonValue value) const
{
    std::shared_ptr<Primitive> result;

    std::string type = value.castField<std::string>("type");
    if (type == "mesh")
        result = std::make_shared<TriangleMesh>();
    else if (type == "sphere")
        result = std::make_shared<Sphere>();
    else if (type == "quad")
        result = std::make_shared<Quad>();
    else if (type == "disk")
        result = std::make_shared<Disk>();
    else if (type == "infinite_sphere")
        result = std::make_shared<InfiniteSphere>();
    else if (type == "infinite_sphere_cap")
        result = std::make_shared<InfiniteSphereCap>();
    else if (type == "curves")
        result = std::make_shared<Curves>();
    else if (type == "cube")
        result = std::make_shared<Cube>();
    else if (type == "minecraft_map")
        result = std::make_shared<MinecraftLoader::TraceableMinecraftMap>();
    else if (type == "skydome")
        result = std::make_shared<Skydome>();
    else if (type == "point")
        result = std::make_shared<Point>();
    else {
        DBG("Unknown primitive type: '%s'", type.c_str());
        return nullptr;
    }

    result->fromJson(value, *this);
    return result;
}

std::shared_ptr<Camera> Scene::instantiateCamera(JsonValue value) const
{
    std::shared_ptr<Camera> result;

    std::string type = value.castField<std::string>("type");
    if (type == "pinhole")
        result = std::make_shared<PinholeCamera>();
    else if (type == "thinlens")
        result = std::make_shared<ThinlensCamera>();
    else if (type == "equirectangular")
        result = std::make_shared<EquirectangularCamera>();
    else if (type == "cubemap")
        result = std::make_shared<CubemapCamera>();
    else {
        DBG("Unknown camera type: '%s'", type.c_str());
        return nullptr;
    }

    result->fromJson(value, *this);
    return result;
}

std::shared_ptr<Integrator> Scene::instantiateIntegrator(JsonValue value) const
{
    std::shared_ptr<Integrator> result;

    std::string type = value.castField<std::string>("type");
    if (type == "path_tracer")
        result = std::make_shared<PathTraceIntegrator>();
    else if (type == "light_tracer")
        result = std::make_shared<LightTraceIntegrator>();
    else if (type == "photon_map")
        result = std::make_shared<PhotonMapIntegrator>();
    else if (type == "progressive_photon_map")
        result = std::make_shared<ProgressivePhotonMapIntegrator>();
    else if (type == "bidirectional_path_tracer")
        result = std::make_shared<BidirectionalPathTraceIntegrator>();
    else if (type == "kelemen_mlt")
        result = std::make_shared<KelemenMltIntegrator>();
    else {
        DBG("Unknown integrator type: '%s'", type.c_str());
        return nullptr;
    }

    result->fromJson(value, *this);
    return result;
}

std::shared_ptr<Texture> Scene::instantiateTexture(JsonValue value, TexelConversion conversion) const
{
    std::shared_ptr<Texture> result;

    std::string type = value.castField<std::string>("type");
    if (type == "bitmap")
        return _textureCache->fetchTexture(value, conversion, this);
    else if (type == "constant")
        result = std::make_shared<ConstantTexture>();
    else if (type == "checker")
        result = std::make_shared<CheckerTexture>();
    else if (type == "disk")
        result = std::make_shared<DiskTexture>();
    else if (type == "blade")
        result = std::make_shared<BladeTexture>();
    else if (type == "ies")
        return _textureCache->fetchIesTexture(value, this);
    else {
        DBG("Unkown texture type: '%s'", type.c_str());
        return nullptr;
    }

    result->fromJson(value, *this);
    return result;
}

template<typename T>
std::shared_ptr<T> Scene::findObject(const std::vector<std::shared_ptr<T>> &list, std::string name) const
{
    for (const std::shared_ptr<T> &t : list)
        if (t->name() == name)
            return t;
    FAIL("Unable to find object '%s'", name.c_str());
    return nullptr;
}

template<typename T, typename Instantiator>
std::shared_ptr<T> Scene::fetchObject(const std::vector<std::shared_ptr<T>> &list, JsonValue value, Instantiator instantiator) const
{
    if (value.isString()) {
        return findObject(list, value.cast<std::string>());
    } else if (value.isObject()) {
        return instantiator(value);
    } else {
        FAIL("Unkown value type");
        return nullptr;
    }
}

std::shared_ptr<PhaseFunction> Scene::fetchPhase(JsonValue value) const
{
    return instantiatePhase(value);
}

std::shared_ptr<Medium> Scene::fetchMedium(JsonValue value) const
{
    return fetchObject(_media, value, std::bind(&Scene::instantiateMedium, this, std::placeholders::_1));
}

std::shared_ptr<Grid> Scene::fetchGrid(JsonValue value) const
{
    return instantiateGrid(value);
}

std::shared_ptr<Bsdf> Scene::fetchBsdf(JsonValue value) const
{
    using namespace std::placeholders;
    auto result = fetchObject(_bsdfs, value, std::bind(&Scene::instantiateBsdf, this, std::placeholders::_1));
    if (!result)
        return _errorBsdf;
    return std::move(result);
}

std::shared_ptr<Texture> Scene::fetchTexture(JsonValue value, TexelConversion conversion) const
{
    // Note: TexelConversions are only honored by BitmapTexture.
    // This is inconsistent, but conversions do not really make sense for other textures,
    // unless the user expects e.g. a ConstantTexture with Vec3 argument to select the green
    // channel when used in a TransparencyBsdf.
    if (value.isString())
        return _textureCache->fetchTexture(fetchResource(value), conversion);
    else if (value.isNumber())
        return std::make_shared<ConstantTexture>(value.cast<float>());
    else if (value.isArray())
        return std::make_shared<ConstantTexture>(value.cast<Vec3f>());
    else if (value.isObject())
        return instantiateTexture(value, conversion);
    else
        DBG("Cannot instantiate texture from unknown value type");
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

PathPtr Scene::fetchResource(JsonValue value) const
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
bool Scene::addUnique(const std::shared_ptr<T> &o, std::vector<std::shared_ptr<T>> &list)
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

void Scene::fromJson(JsonValue value, const Scene &scene)
{
    JsonSerializable::fromJson(value, scene);

    if (auto media = value["media"])
        for (unsigned i = 0; i < media.size(); ++i)
            _media.emplace_back(instantiateMedium(media[i]));
    if (auto bsdfs = value["bsdfs"])
        for (unsigned i = 0; i < bsdfs.size(); ++i)
            _bsdfs.emplace_back(instantiateBsdf(bsdfs[i]));
    if (auto primitives = value["primitives"])
        for (unsigned i = 0; i < primitives.size(); ++i)
            _primitives.emplace_back(instantiatePrimitive(primitives[i]));

    if (auto camera     = value["camera"    ]) _camera     = instantiateCamera(camera);
    if (auto integrator = value["integrator"]) _integrator = instantiateIntegrator(integrator);
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

Scene *Scene::load(const Path &path, std::shared_ptr<TextureCache> cache)
{
    JsonDocument document(path);

    DirectoryChange context(path.parent());
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
