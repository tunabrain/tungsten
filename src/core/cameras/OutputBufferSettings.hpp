#ifndef OUTPUTBUFFERSETTINGS_HPP_
#define OUTPUTBUFFERSETTINGS_HPP_

#include "io/JsonSerializable.hpp"
#include "io/Path.hpp"

#include "StringableEnum.hpp"

namespace Tungsten {

enum OutputBufferTypeEnum
{
    OutputColor      = 0,
    OutputDepth      = 1,
    OutputNormal     = 2,
    OutputAlbedo     = 3,
    OutputVisibility = 4,
};

class OutputBufferSettings : public JsonSerializable
{
    typedef StringableEnum<OutputBufferTypeEnum> Type;
    friend Type;

    Type _type;
    Path _ldrOutputFile;
    Path _hdrOutputFile;
    Path _outputDirectory;
    bool _twoBufferVariance;
    bool _sampleVariance;

public:
    OutputBufferSettings();

    virtual void fromJson(JsonPtr value, const Scene &/*scene*/) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    void setOutputDirectory(const Path &directory);

    void setType(OutputBufferTypeEnum type)
    {
        _type = type;
    }

    const char *typeString() const
    {
        return _type.toString();
    }

    Type type() const
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
