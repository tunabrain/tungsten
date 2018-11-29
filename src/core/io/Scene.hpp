#ifndef SCENE_HPP_
#define SCENE_HPP_

#include <rapidjson/document.h>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <vector>
#include <map>

#include "JsonSerializable.hpp"
#include "TextureCache.hpp"
#include "ImageIO.hpp"
#include "Path.hpp"

#include "transmittances/Transmittance.hpp"

#include "phasefunctions/PhaseFunction.hpp"

#include "integrators/Integrator.hpp"

#include "primitives/Primitive.hpp"

#include "renderer/RendererSettings.hpp"
#include "renderer/TraceableScene.hpp"

#include "cameras/Camera.hpp"

#include "media/Medium.hpp"

#include "grids/Grid.hpp"

#include "bsdfs/Bsdf.hpp"

namespace Tungsten {

class Scene : public JsonSerializable
{
    Path _srcDir;
    Path _path;

    std::vector<std::shared_ptr<Primitive>> _primitives;
    std::vector<std::shared_ptr<Medium>> _media;
    std::vector<std::shared_ptr<Bsdf>> _bsdfs;
    std::shared_ptr<TextureCache> _textureCache;
    std::shared_ptr<Camera> _camera;
    std::shared_ptr<Integrator> _integrator;

    std::unordered_set<const Primitive *> _helperPrimitives;
    mutable std::unordered_map<Path, PathPtr> _resources;

    RendererSettings _rendererSettings;

public:
    Scene();

    Scene(const Path &srcDir, std::shared_ptr<TextureCache> cache);

    Scene(const Path &srcDir,
          std::vector<std::shared_ptr<Primitive>> primitives,
          std::vector<std::shared_ptr<Bsdf>> bsdfs,
          std::shared_ptr<TextureCache> cache,
          std::shared_ptr<Camera> camera);

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual void loadResources() override;
    virtual void saveResources() override;

    std::shared_ptr<Transmittance> fetchTransmittance(JsonPtr value) const;
    std::shared_ptr<PhaseFunction> fetchPhase(JsonPtr value) const;
    std::shared_ptr<Medium> fetchMedium(JsonPtr value) const;
    std::shared_ptr<Grid> fetchGrid(JsonPtr value) const;
    std::shared_ptr<Bsdf> fetchBsdf(JsonPtr value) const;
    std::shared_ptr<Texture> fetchTexture(JsonPtr value, TexelConversion conversion) const;
    PathPtr fetchResource(const std::string &path) const;
    PathPtr fetchResource(JsonPtr v) const;

    const Primitive *findPrimitive(const std::string &name) const;

    void deletePrimitives(const std::unordered_set<Primitive *> &primitives);
    void deleteBsdfs(const std::unordered_set<Bsdf *> &bsdfs);
    void deleteMedia(const std::unordered_set<Medium *> &media);

    void pruneBsdfs();
    void pruneMedia();

    void addPrimitive(const std::shared_ptr<Primitive> &mesh);
    void addBsdf(const std::shared_ptr<Bsdf> &bsdf);

    void merge(Scene scene);

    TraceableScene *makeTraceable(uint32 seed = 0xBA5EBA11);

    std::vector<std::shared_ptr<Medium>> &media()
    {
        return _media;
    }

    std::vector<std::shared_ptr<Bsdf>> &bsdfs()
    {
        return _bsdfs;
    }

    const std::vector<std::shared_ptr<Primitive>> &primitives() const
    {
        return _primitives;
    }

    std::vector<std::shared_ptr<Primitive>> &primitives()
    {
        return _primitives;
    }

    void setCamera(Camera *cam)
    {
        _camera.reset(cam);
    }

    std::shared_ptr<Camera> camera()
    {
        return _camera;
    }

    const std::shared_ptr<TextureCache> textureCache() const
    {
        return _textureCache;
    }

    const std::shared_ptr<Camera> camera() const
    {
        return _camera;
    }

    void setPath(const Path &p)
    {
        _path = p;
    }

    const Path &path() const
    {
        return _path;
    }

    const RendererSettings &rendererSettings() const
    {
        return _rendererSettings;
    }

    RendererSettings &rendererSettings()
    {
        return _rendererSettings;
    }
    
    Integrator *integrator()
    {
        return _integrator.get();
    }

    std::unordered_map<Path, PathPtr> &resources()
    {
        return _resources;
    }

    static Scene *load(const Path &path, std::shared_ptr<TextureCache> cache = nullptr);
    static void save(const Path &path, const Scene &scene);
};

}


#endif /* SCENE_HPP_ */
