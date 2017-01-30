#ifndef IESTEXTURE_HPP_
#define IESTEXTURE_HPP_

#include "BitmapTexture.hpp"

namespace Tungsten {

class IesTexture : public BitmapTexture
{
    PathPtr _path;
    int _resolution;

    typedef JsonSerializable::Allocator Allocator;

public:
    IesTexture();
    IesTexture(PathPtr path, int resolution);

    virtual void fromJson(JsonPtr value, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual void loadResources() override;

    virtual Texture *clone() const override;

    const PathPtr &path() const
    {
        return _path;
    }

    int resolution() const
    {
        return _resolution;
    }

    bool operator<(const IesTexture &o) const
    {
        return _path != o._path ? _path < o._path :
            _resolution != o._resolution ? _resolution < o._resolution :
            false;
    }

    bool operator==(const IesTexture &o) const
    {
        return _path == o._path && _resolution == o._resolution;
    }
};

}

#endif /* IESTEXTURE_HPP_ */
