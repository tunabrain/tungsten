#ifndef RENDERERSETTINGS_HPP_
#define RENDERERSETTINGS_HPP_

#include "cameras/OutputBufferSettings.hpp"

#include "io/JsonSerializable.hpp"
#include "io/DirectoryChange.hpp"
#include "io/JsonObject.hpp"
#include "io/FileUtils.hpp"

namespace Tungsten {

class Scene;

class RendererSettings : public JsonSerializable
{
    Path _outputDirectory;
    Path _outputFile;
    Path _hdrOutputFile;
    Path _varianceOutputFile;
    Path _resumeRenderFile;
    bool _overwriteOutputFiles;
    bool _useAdaptiveSampling;
    bool _enableResumeRender;
    bool _useSceneBvh;
    bool _useSobol;
    uint32 _spp;
    uint32 _sppStep;
    std::string _checkpointInterval;
    std::string _timeout;
    std::vector<OutputBufferSettings> _outputs;

public:
    RendererSettings()
    : _outputFile("TungstenRender.png"),
      _resumeRenderFile("TungstenRenderState.dat"),
      _overwriteOutputFiles(true),
      _useAdaptiveSampling(true),
      _enableResumeRender(false),
      _useSceneBvh(true),
      _useSobol(true),
      _spp(32),
      _sppStep(16),
      _checkpointInterval("0"),
      _timeout("0")
    {
    }

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene)
    {
        JsonUtils::fromJson(v, "output_directory", _outputDirectory);

        _outputDirectory.freezeWorkingDirectory();
        DirectoryChange change(_outputDirectory);

        JsonUtils::fromJson(v, "output_file", _outputFile);
        JsonUtils::fromJson(v, "hdr_output_file", _hdrOutputFile);
        JsonUtils::fromJson(v, "variance_output_file", _varianceOutputFile);
        JsonUtils::fromJson(v, "resume_render_file", _resumeRenderFile);
        JsonUtils::fromJson(v, "overwrite_output_files", _overwriteOutputFiles);
        JsonUtils::fromJson(v, "adaptive_sampling", _useAdaptiveSampling);
        JsonUtils::fromJson(v, "enable_resume_render", _enableResumeRender);
        JsonUtils::fromJson(v, "stratified_sampler", _useSobol);
        JsonUtils::fromJson(v, "scene_bvh", _useSceneBvh);
        JsonUtils::fromJson(v, "spp", _spp);
        JsonUtils::fromJson(v, "spp_step", _sppStep);
        JsonUtils::fromJson(v, "checkpoint_interval", _checkpointInterval);
        JsonUtils::fromJson(v, "timeout", _timeout);

        auto outputs = v.FindMember("output_buffers");
        if (outputs != v.MemberEnd() && outputs->value.IsArray()) {
            for (rapidjson::SizeType i = 0; i < outputs->value.Size(); ++i) {
                _outputs.emplace_back();
                _outputs.back().fromJson(outputs->value[i], scene);
            }
        }
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const
    {
        JsonObject result{JsonSerializable::toJson(allocator), allocator,
            "overwrite_output_files", _overwriteOutputFiles,
            "adaptive_sampling", _useAdaptiveSampling,
            "enable_resume_render", _enableResumeRender,
            "stratified_sampler", _useSobol,
            "scene_bvh", _useSceneBvh,
            "spp", _spp,
            "spp_step", _sppStep,
            "checkpoint_interval", _checkpointInterval,
            "timeout", _timeout
        };
        if (!_outputFile.empty())
            result.add("output_file", _outputFile);
        if (!_hdrOutputFile.empty())
            result.add("hdr_output_file", _hdrOutputFile);
        if (!_varianceOutputFile.empty())
            result.add("variance_output_file", _varianceOutputFile);
        if (!_resumeRenderFile.empty())
            result.add("resume_render_file", _resumeRenderFile);
        if (!_outputs.empty()) {
            rapidjson::Value outputs(rapidjson::kArrayType);
            for (const auto &b : _outputs)
                outputs.PushBack(b.toJson(allocator), allocator);
            result.add("output_buffers", std::move(outputs));
        }

        return result;
    }

    const Path &outputDirectory() const
    {
        return _outputDirectory;
    }

    void setOutputDirectory(const Path &directory)
    {
        _outputDirectory = directory;

        _outputFile        .setWorkingDirectory(_outputDirectory);
        _hdrOutputFile     .setWorkingDirectory(_outputDirectory);
        _varianceOutputFile.setWorkingDirectory(_outputDirectory);
        _resumeRenderFile  .setWorkingDirectory(_outputDirectory);

        for (auto &b : _outputs)
            b.setOutputDirectory(_outputDirectory);
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

    const Path &resumeRenderFile() const
    {
        return _resumeRenderFile;
    }

    bool overwriteOutputFiles() const
    {
        return _overwriteOutputFiles;
    }

    bool useAdaptiveSampling() const
    {
        return _useAdaptiveSampling;
    }

    bool enableResumeRender() const
    {
        return _enableResumeRender;
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

    std::string checkpointInterval() const
    {
        return _checkpointInterval;
    }

    std::string timeout() const
    {
        return _timeout;
    }

    const std::vector<OutputBufferSettings> &renderOutputs() const
    {
        return _outputs;
    }

    void setUseSceneBvh(bool value)
    {
        _useSceneBvh = value;
    }

    void setSpp(uint32 spp)
    {
        _spp = spp;
    }

    void setSppStep(uint32 step)
    {
        _sppStep = step;
    }
};

}

#endif /* RENDERERSETTINGS_HPP_ */
