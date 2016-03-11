#ifndef OUTPUTBUFFERSETTINGS_HPP_
#define OUTPUTBUFFERSETTINGS_HPP_

#include "io/JsonSerializable.hpp"
#include "io/JsonObject.hpp"
#include "io/FileUtils.hpp"

namespace Tungsten {

enum OutputBufferType
{
    OutputColor      = 0,
    OutputDepth      = 1,
    OutputNormal     = 2,
    OutputAlbedo     = 3,
    OutputVisibility = 4,
    OutputUnknown    = 5,
};

class OutputBufferSettings : public JsonSerializable
{
    std::string _typeString;
    OutputBufferType _type;
    Path _ldrOutputFile;
    Path _hdrOutputFile;
    Path _outputDirectory;
    bool _twoBufferVariance;
    bool _sampleVariance;

    static std::string typeToString(OutputBufferType type)
    {
        switch (type) {
        case OutputColor:      return "color";
        case OutputDepth:      return "depth";
        case OutputNormal:     return "normal";
        case OutputAlbedo:     return "albedo";
        case OutputVisibility: return "visibility";
        default:               return "unknown";
        }
    }

    static OutputBufferType stringToType(const std::string &typeString)
    {
        if (typeString == "color")
            return OutputColor;
        else if (typeString == "depth")
            return OutputDepth;
        else if (typeString == "normal")
            return OutputNormal;
        else if (typeString == "albedo")
            return OutputAlbedo;
        else if (typeString == "visibility")
            return OutputVisibility;
        return OutputUnknown;
    }

public:
    OutputBufferSettings()
    : _type(OutputUnknown),
      _twoBufferVariance(false),
      _sampleVariance(false)
    {
    }

    virtual void fromJson(const rapidjson::Value &v, const Scene &/*scene*/) override
    {
        if (!JsonUtils::fromJson(v, "type", _typeString)) {
            DBG("Warning: Missing output buffer type");
        } else {
            _type = stringToType(_typeString);
            if (_type == OutputUnknown)
                DBG("Warning: Unknown output buffer type '%s'", _typeString);
        }
        JsonUtils::fromJson(v, "ldr_output_file", _ldrOutputFile);
        JsonUtils::fromJson(v, "hdr_output_file", _hdrOutputFile);
        JsonUtils::fromJson(v, "two_buffer_variance", _twoBufferVariance);
        JsonUtils::fromJson(v, "sample_variance", _sampleVariance);
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const override
    {
        JsonObject result{JsonSerializable::toJson(allocator), allocator,
            "two_buffer_variance", _twoBufferVariance,
            "sample_variance", _sampleVariance
        };
        if (!_typeString.empty())
            result.add("type", _typeString);
        if (!_ldrOutputFile.empty())
            result.add("output_file", _ldrOutputFile);
        if (!_hdrOutputFile.empty())
            result.add("hdr_output_file", _hdrOutputFile);

        return result;
    }

    void setOutputDirectory(const Path &directory)
    {
        _outputDirectory = directory;

        _ldrOutputFile.setWorkingDirectory(_outputDirectory);
        _hdrOutputFile.setWorkingDirectory(_outputDirectory);
    }

    void setType(OutputBufferType type)
    {
        _type = type;
    }

    const std::string &typeString() const
    {
        return _typeString;
    }

    OutputBufferType type() const
    {
        return _type;
    }

    bool twoBufferVariance() const
    {
        return _twoBufferVariance;
    }

    bool sampleVariance() const
    {
        return _sampleVariance;
    }

    const Path &hdrOutputFile() const
    {
        return _hdrOutputFile;
    }

    const Path &ldrOutputFile() const
    {
        return _ldrOutputFile;
    }

};

}

#endif /* OUTPUTBUFFERSETTINGS_HPP_ */
