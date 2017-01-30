#include "JsonUtils.hpp"
#include "Path.hpp"

#include <rapidjson/prettywriter.h>
#include <sstream>
#include <cstdlib>
#include <cstdio>

namespace Tungsten {

namespace JsonUtils {

// If the float falls within 10^-6 of a prettier number, we round it to that
// Here, a "pretty number" is one with less than three places after the decimal point
// Also takes care of -0
static float prettifyFloat(float f)
{
    if (std::abs(int(f*100.0f) - f*100.0f) < 10e-4f)
        f = int(f*100.0f)*0.01f;
    // Get rid of negative zero
    if (f == 0.0f)
        f = 0.0f;
    return f;
}

// Ok so this is super evil
//
// The basic problem is that rapidjson doesn't have a float type, only double.
// However, internally Tungsten uses mostly floats.
// Although not immediately obvious, this is a usability problem.
//
// Imagine the user specifies a value of 0.1 in the JSON file. Tungsten will internally
// convert this into a float representation that's very close to this value
// (because 0.1 cannot be represented exactly in binary)
// When you print the float, this would give you back a string representation corresponding to 0.1
// (which is what the user entered)
// However, if you convert the float representation of 0.1 to double first and then print it, you will
// get a different string representation (namely 0.09999999776482582). This is because doubles
// are printed with higher precision.
//
// This is a problem, because if a user specifies 0.1 in the JSON, and tungsten writes the JSON
// back out, the value will now read 0.09999999776482582. This is because it converts to float
// internally on load and then converts back to double when passing it to rapidjson on save.
// This is terrible!
//
// So instead of converting float to double using the native conversion, we instead use a conversion
// that preserves the same string representation - in other words, we print the float to a string,
// and convert that string to a double. This ensures the user gets back exactly what they entered.
//
// This is really really bad from a performance perspective, so if this becomes a bottleneck,
// we need a better way of doing this.
static double prettifyFloatToDouble(float f)
{
    char tmp[1024];
    std::sprintf(tmp, "%f", f);
    return std::atof(tmp);
}

static Vec3f prettifyVector(const Vec3f &p)
{
    return Vec3f(
        prettifyFloat(p.x()),
        prettifyFloat(p.y()),
        prettifyFloat(p.z())
    );
}

rapidjson::Value toJson(rapidjson::Value v, rapidjson::Document::AllocatorType &/*allocator*/)
{
    return std::move(v);
}

rapidjson::Value toJson(const JsonSerializable &o, rapidjson::Document::AllocatorType &allocator)
{
    return o.unnamed() ? o.toJson(allocator) : toJson(o.name(), allocator);
}

rapidjson::Value toJson(const std::string &value, rapidjson::Document::AllocatorType &allocator)
{
    rapidjson::Value result;
    result.SetString(value.c_str(), value.size(), allocator);
    return std::move(result);
}

rapidjson::Value toJson(const char *value, rapidjson::Document::AllocatorType &allocator)
{
    rapidjson::Value result;
    result.SetString(value, allocator);
    return std::move(result);
}

rapidjson::Value toJson(const Path &value, rapidjson::Document::AllocatorType &allocator)
{
    return toJson(value.asString(), allocator);
}

rapidjson::Value toJson(bool value, rapidjson::Document::AllocatorType &/*allocator*/)
{
    return rapidjson::Value(value);
}

rapidjson::Value toJson(uint32 value, rapidjson::Document::AllocatorType &/*allocator*/)
{
    return rapidjson::Value(value);
}

rapidjson::Value toJson(int32 value, rapidjson::Document::AllocatorType &/*allocator*/)
{
    return rapidjson::Value(value);
}

rapidjson::Value toJson(uint64_t value, rapidjson::Document::AllocatorType &/*allocator*/)
{
    return rapidjson::Value(value);
}

rapidjson::Value toJson(float value, rapidjson::Document::AllocatorType &/*allocator*/)
{
    return rapidjson::Value(prettifyFloatToDouble(value));
}

rapidjson::Value toJson(double value, rapidjson::Document::AllocatorType &/*allocator*/)
{
    return rapidjson::Value(value);
}

rapidjson::Value toJson(const Mat4f &value, rapidjson::Document::AllocatorType &allocator)
{
    Vec3f   rot = prettifyVector(value.extractRotationVec());
    Vec3f scale = prettifyVector(value.extractScaleVec());
    Vec3f   pos = prettifyVector(value.extractTranslationVec());

    if (value.right().cross(value.up()).dot(value.fwd()) < 0.0f) {
        rot = prettifyVector((value*Mat4f::scale(Vec3f(1.0f, 1.0f, -1.0f))).extractRotationVec());
        scale.z() *= -1.0f;
    }

    rapidjson::Value a(rapidjson::kObjectType);
    if (pos != 0.0f)
        a.AddMember("position", toJson(pos, allocator), allocator);
    if (scale != 1.0f)
        a.AddMember("scale", toJson(scale, allocator), allocator);
    if (rot != 0.0f)
        a.AddMember("rotation", toJson(rot, allocator), allocator);

    return std::move(a);
}

void addObjectMember(rapidjson::Value &v, const char *name, const JsonSerializable &o,
        rapidjson::Document::AllocatorType &allocator)
{
    if (o.unnamed())
        v.AddMember(rapidjson::StringRef(name), o.toJson(allocator), allocator);
    else
        v.AddMember(rapidjson::StringRef(name), toJson(o.name(), allocator), allocator);
}

struct JsonStringWriter {
    std::stringstream sstream;
    typedef char Ch;
    void Put(char c) { sstream.put(c); }
    void PutN(char c, size_t n) { for (size_t i = 0; i < n; ++i) sstream.put(c); }
    void Flush() { sstream.flush(); }
};
std::string jsonToString(const rapidjson::Document &document)
{
    JsonStringWriter out;
    rapidjson::PrettyWriter<JsonStringWriter> writer(out);
    document.Accept(writer);
    // Yes, the string is copied here. stringstream does not allow moving the result out (boo!)
    return out.sstream.str();
}

}

}
