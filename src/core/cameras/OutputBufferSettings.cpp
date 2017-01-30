#include "OutputBufferSettings.hpp"

#include "io/JsonObject.hpp"
#include "io/FileUtils.hpp"

namespace Tungsten {

DEFINE_STRINGABLE_ENUM(OutputBufferSettings::Type, "output buffer type", ({
    {"color", OutputColor},
    {"depth", OutputDepth},
    {"normal", OutputNormal},
    {"albedo", OutputAlbedo},
    {"visibility", OutputVisibility}
}))

OutputBufferSettings::OutputBufferSettings()
: _type("color"),
  _twoBufferVariance(false),
  _sampleVariance(false)
{
}

void OutputBufferSettings::fromJson(JsonPtr value, const Scene &/*scene*/)
{
    _type = value.getRequiredMember("type");
    value.getField("ldr_output_file", _ldrOutputFile);
    value.getField("hdr_output_file", _hdrOutputFile);
    value.getField("two_buffer_variance", _twoBufferVariance);
    value.getField("sample_variance", _sampleVariance);
}

rapidjson::Value OutputBufferSettings::toJson(Allocator &allocator) const
{
    JsonObject result{JsonSerializable::toJson(allocator), allocator,
        "two_buffer_variance", _twoBufferVariance,
        "sample_variance", _sampleVariance
    };
    result.add("type", _type.toString());
    if (!_ldrOutputFile.empty())
        result.add("output_file", _ldrOutputFile);
    if (!_hdrOutputFile.empty())
        result.add("hdr_output_file", _hdrOutputFile);

    return result;
}

void OutputBufferSettings::setOutputDirectory(const Path &directory)
{
    _outputDirectory = directory;

    _ldrOutputFile.setWorkingDirectory(_outputDirectory);
    _hdrOutputFile.setWorkingDirectory(_outputDirectory);
}

}
