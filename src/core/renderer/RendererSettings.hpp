#ifndef RENDERERSETTINGS_HPP_
#define RENDERERSETTINGS_HPP_

#include "io/JsonSerializable.hpp"
#include "io/JsonUtils.hpp"

namespace Tungsten {

class Scene;

class RendererSettings : public JsonSerializable
{
    Path _outputFile;
    Path _hdrOutputFile;
    Path _varianceOutputFile;
    bool _overwriteOutputFiles;
    bool _useAdaptiveSampling;
    bool _useSceneBvh;
    bool _useSobol;
    uint32 _spp;
    uint32 _sppStep;

public:
    RendererSettings()
    : _outputFile("TungstenRender.png"),
      _overwriteOutputFiles(true),
      _useAdaptiveSampling(true),
      _useSceneBvh(true),
      _useSobol(true),
      _spp(32),
      _sppStep(16)
    {
    }

    virtual void fromJson(const rapidjson::Value &v, const Scene &/*scene*/)
    {
        JsonUtils::fromJson(v, "output_file", _outputFile);
        JsonUtils::fromJson(v, "hdr_output_file", _hdrOutputFile);
        JsonUtils::fromJson(v, "variance_output_file", _varianceOutputFile);
        JsonUtils::fromJson(v, "overwrite_output_files", _overwriteOutputFiles);
        JsonUtils::fromJson(v, "adaptive_sampling", _useAdaptiveSampling);
        JsonUtils::fromJson(v, "stratified_sampler", _useSobol);
        JsonUtils::fromJson(v, "scene_bvh", _useSceneBvh);
        JsonUtils::fromJson(v, "spp", _spp);
        JsonUtils::fromJson(v, "spp_step", _sppStep);
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const
    {
        rapidjson::Value v(JsonSerializable::toJson(allocator));
        if (!_outputFile.empty())
            v.AddMember("output_file", _outputFile.asString().c_str(), allocator);
        if (!_hdrOutputFile.empty())
            v.AddMember("hdr_output_file", _hdrOutputFile.asString().c_str(), allocator);
        if (!_varianceOutputFile.empty())
            v.AddMember("variance_output_file", _varianceOutputFile.asString().c_str(), allocator);
        v.AddMember("overwrite_output_files", _overwriteOutputFiles, allocator);
        v.AddMember("adaptive_sampling", _useAdaptiveSampling, allocator);
        v.AddMember("stratified_sampler", _useSobol, allocator);
        v.AddMember("scene_bvh", _useSceneBvh, allocator);
        v.AddMember("spp", _spp, allocator);
        v.AddMember("spp_step", _sppStep, allocator);
        return std::move(v);
    }

    const Path &outputFile() const
    {
        return _outputFile;
    }

    const Path &hdrOutputFile() const
    {
        return _hdrOutputFile;
    }

    const Path &varianceOutputFile() const
    {
        return _varianceOutputFile;
    }

    bool overwriteOutputFiles() const
    {
        return _overwriteOutputFiles;
    }

    bool useAdaptiveSampling() const
    {
        return _useAdaptiveSampling;
    }

    bool useSobol() const
    {
        return _useSobol;
    }

    bool useSceneBvh() const
    {
        return _useSceneBvh;
    }

    uint32 spp() const
    {
        return _spp;
    }

    uint32 sppStep() const
    {
        return _sppStep;
    }
};

}

#endif /* RENDERERSETTINGS_HPP_ */
