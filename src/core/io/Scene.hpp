#ifndef SCENE_HPP_
#define SCENE_HPP_

#include <rapidjson/document.h>
#include <unordered_set>
#include <memory>
#include <vector>
#include <map>

#include "JsonSerializable.hpp"

#include "materials/BitmapTexture.hpp"

#include "primitives/Primitive.hpp"

#include "cameras/Camera.hpp"

#include "volume/Medium.hpp"

#include "bsdfs/Bsdf.hpp"

#include "TraceableScene.hpp"

namespace Tungsten
{

class Scene : public JsonSerializable
{
    std::string _srcDir;
    std::string _path;

    std::vector<std::shared_ptr<Primitive>> _primitives;
    std::vector<std::shared_ptr<Medium>> _media;
    std::vector<std::shared_ptr<Bsdf>> _bsdfs;
    mutable std::map<std::string, std::shared_ptr<BitmapTextureRgb>> _colorMaps;
    mutable std::map<std::string, std::shared_ptr<BitmapTextureA>> _scalarMaps;
    std::shared_ptr<Camera> _camera;

    std::shared_ptr<Medium>    instantiateMedium   (std::string type, const rapidjson::Value &value) const;
    std::shared_ptr<Bsdf>      instantiateBsdf     (std::string type, const rapidjson::Value &value) const;
    std::shared_ptr<Primitive> instantiatePrimitive(std::string type, const rapidjson::Value &value) const;
    std::shared_ptr<Camera>    instantiateCamera   (std::string type, const rapidjson::Value &value) const;
    template<int Dimension>
    std::shared_ptr<Texture<true, Dimension>>
        instantiateScalarTexture(std::string type, const rapidjson::Value &value) const;
    template<int Dimension>
    std::shared_ptr<Texture<false, Dimension>>
        instantiateColorTexture(std::string type, const rapidjson::Value &value) const;

    template<typename Instantiator, typename Element>
    void loadObjectList(const rapidjson::Value &container, Instantiator instantiator, std::vector<std::shared_ptr<Element>> &result);

    template<typename T>
    std::shared_ptr<T> findObject(const std::vector<std::shared_ptr<T>> &list, std::string name) const;

    template<typename T, typename Instantiator>
    std::shared_ptr<T> fetchObject(const std::vector<std::shared_ptr<T>> &list, const rapidjson::Value &v, Instantiator instantiator) const;

    template<typename T>
    bool addUnique(const std::shared_ptr<T> &o, std::vector<std::shared_ptr<T>> &list);

    template<typename T1, typename T2>
    void addTexture(std::shared_ptr<T1> &t, std::map<std::string, std::shared_ptr<T2>> &maps);

public:
    Scene() = default;

    Scene(const std::string &srcDir);

    Scene(const std::string &srcDir,
          std::vector<std::shared_ptr<Primitive>> primitives,
          std::vector<std::shared_ptr<Bsdf>> bsdfs,
          std::shared_ptr<Camera> camera);

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene);
    virtual rapidjson::Value toJson(Allocator &allocator) const;
    void saveData(const std::string &dst) const;

    std::shared_ptr<Medium> fetchMedium(const rapidjson::Value &v) const;
    std::shared_ptr<Bsdf> fetchBsdf(const rapidjson::Value &v) const;
    template<int Dimension>
    std::shared_ptr<Texture<true, Dimension>> fetchScalarTexture(const rapidjson::Value &v) const;
    template<int Dimension>
    std::shared_ptr<Texture<false, Dimension>> fetchColorTexture(const rapidjson::Value &v) const;
    std::shared_ptr<TextureRgb> fetchColorMap(const std::string &path) const;
    std::shared_ptr<TextureA> fetchScalarMap(const std::string &path) const;

    void deletePrimitives(const std::unordered_set<Primitive *> &primitives);

    void addPrimitive(const std::shared_ptr<Primitive> &mesh);
    void addBsdf(const std::shared_ptr<Bsdf> &bsdf);

    void merge(Scene scene);

    TraceableScene *makeTraceable();

    std::vector<std::shared_ptr<Medium>> &media()
    {
        return _media;
    }

    std::vector<std::shared_ptr<Bsdf>> &bsdfs()
    {
        return _bsdfs;
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

    const std::shared_ptr<Camera> camera() const
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

    static Scene *load(const std::string &path);
    static void save(const std::string &path, const Scene &scene, bool includeData);
};

}


#endif /* SCENE_HPP_ */
