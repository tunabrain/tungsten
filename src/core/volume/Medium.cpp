#include "Medium.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten {

Medium::Medium()
: _phaseFunctionName("isotropic"),
  _phaseG(0.0f),
  _maxBounce(1024)
{
    init();
}

void Medium::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    JsonSerializable::fromJson(v, scene);
    JsonUtils::fromJson(v, "phase_function", _phaseFunctionName);
    JsonUtils::fromJson(v, "phase_g", _phaseG);
    JsonUtils::fromJson(v, "max_bounces", _maxBounce);
    init();
}

rapidjson::Value Medium::toJson(Allocator &allocator) const
{
    rapidjson::Value v(JsonSerializable::toJson(allocator));
    v.AddMember("phase_function", _phaseFunctionName.c_str(), allocator);
    v.AddMember("phase_g", _phaseG, allocator);
    v.AddMember("max_bounces", _maxBounce, allocator);

    return std::move(v);
}

}
