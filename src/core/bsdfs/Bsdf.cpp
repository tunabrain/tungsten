#include "Bsdf.hpp"

#include "io/Scene.hpp"

namespace Tungsten {

void Bsdf::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    JsonSerializable::fromJson(v, scene);

    JsonUtils::fromJson(v, "emission", _emission);
    JsonUtils::fromJson(v, "bumpStrength", _bumpStrength);

    const rapidjson::Value::Member *base  = v.FindMember("color");
    const rapidjson::Value::Member *alpha = v.FindMember("alpha");
    const rapidjson::Value::Member *bump  = v.FindMember("bump");

    if (base)
        _base = scene.fetchColorTexture<2>(base->value);
    if (alpha)
        _alpha = scene.fetchScalarTexture<2>(alpha->value);
    if (bump)
        _bump = scene.fetchScalarTexture<2>(bump->value);
}

}
