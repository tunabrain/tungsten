#include "ReconstructionFilter.hpp"

#include "io/JsonUtils.hpp"

namespace Tungsten {

CONSTEXPR int ReconstructionFilter::Resolution;

ReconstructionFilter::Type ReconstructionFilter::stringToType(const std::string &s)
{
    if (s == "dirac")
        return Dirac;
    if (s == "box")
        return Box;
    if (s == "tent")
        return Tent;
    if (s == "gaussian")
        return Gaussian;
    if (s == "mitchell_netravali")
        return MitchellNetravali;
    if (s == "catmull_rom")
        return CatmullRom;
    if (s == "lanczos")
        return Lanczos;
    if (s == "airy")
        return Airy;
    FAIL("Invalid reconstruction filter: '%s'", s.c_str());
    return Box;
}

float ReconstructionFilter::defaultWidth()
{
    switch (_type) {
    case Dirac:
        return 0.0f;
    case Box:
    case Tent:
        return 1.0f;
    case MitchellNetravali:
    case CatmullRom:
        return 2.0f;
    case Lanczos:
    case Gaussian:
        return 3.0f;
    case Airy:
        return 5.0f;
    }
    return 1.0f;
}

void ReconstructionFilter::precompute()
{
    if (_type == Box || _type == Dirac)
        return;

    // Precompute vertical offset
    if (_type == Gaussian)
        _offset = std::exp(-_alpha*_width*_width);

    _normalizationFactor = 1.0f;
    float evalIntegral = 0.0f;
    std::vector<float> weights(Resolution*Resolution);
    for (int y = 0, idx = 0; y < Resolution; ++y) {
        for (int x = 0; x < Resolution; ++x, ++idx) {
            Vec2f uv(
                ((x + 0.5f)/Resolution*2.0f - 1.0f)*_width,
                ((y + 0.5f)/Resolution*2.0f - 1.0f)*_width
            );
            float value = eval(uv);
            weights[idx] = std::abs(value);
            evalIntegral += value*sqr(2.0f*_width/Resolution);
        }
    }
    _normalizationFactor = 1.0f/evalIntegral;
    _footprint.reset(new Distribution2D(std::move(weights), Resolution, Resolution));
}

void ReconstructionFilter::fromJson(const rapidjson::Value &v)
{
    JsonUtils::fromJson(v, "type", _typeString);
    _type = stringToType(_typeString);
    _width = defaultWidth();
    if (_type == Lanczos || _type == Airy || _type == Gaussian)
        JsonUtils::fromJson(v, "width", _width);
    if (_type == Gaussian)
        JsonUtils::fromJson(v, "alpha", _alpha);

    precompute();
}

rapidjson::Value ReconstructionFilter::toJson(rapidjson::Document::AllocatorType &allocator) const
{
    rapidjson::Value v(rapidjson::kObjectType);
    v.AddMember("type", _typeString.c_str(), allocator);
    if (_type == Lanczos || _type == Airy || _type == Gaussian)
        v.AddMember("width", _width, allocator);
    if (_type == Gaussian)
        v.AddMember("alpha", _alpha, allocator);
    return std::move(v);
}

}
