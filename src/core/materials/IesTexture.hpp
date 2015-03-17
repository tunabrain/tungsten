#ifndef IESTEXTURE_HPP_
#define IESTEXTURE_HPP_

#include "BitmapTexture.hpp"

namespace Tungsten {

class IesTexture : public BitmapTexture
{
    PathPtr _path;
    int _resolution;
    float _scale;

    typedef JsonSerializable::Allocator Allocator;

public:
    IesTexture();
    IesTexture(PathPtr path, int resolution, float scale);

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual void loadResources() override;

    const PathPtr &path() const
    {
        return _path;
    }

    int resolution() const
    {
        return _resolution;
    }

    float scale() const
    {
        return _scale;
    }

    bool operator<(const IesTexture &o) const
    {
        return _path != o._path ? _path < o._path :
            _resolution != o._resolution ? _resolution < o._resolution :
            _scale != o._scale ? _scale < o._scale:
            false;
    }

    bool operator==(const IesTexture &o) const
    {
        return _path == o._path && _resolution == o._resolution && _scale == o._scale;
    }
};

}

#endif /* IESTEXTURE_HPP_ */
