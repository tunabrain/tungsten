#include "JsonUtils.hpp"
#include "Path.hpp"

#include <rapidjson/prettywriter.h>
#include <sstream>

namespace Tungsten {

namespace JsonUtils {

const rapidjson::Value &fetchMember(const rapidjson::Value &v, const char *name)
{
    const rapidjson::Value::Member *member = v.FindMember(name);
    ASSERT(member != nullptr, "Json value is missing mandatory member '%s'", name);
    return member->value;
}

bool fromJson(const rapidjson::Value &v, bool &dst)
{
    if (v.IsBool()) {
        dst = v.GetBool();
        return true;
    }
    return false;
}

template<typename T>
bool getJsonNumber(const rapidjson::Value &v, T &dst) {
    if (v.IsDouble())
        dst = T(v.GetDouble());
    else if (v.IsInt())
        dst = T(v.GetInt());
    else if (v.IsUint())
        dst = T(v.GetUint());
    else if (v.IsInt64())
        dst = T(v.GetInt64());
    else if (v.IsUint64())
        dst = T(v.GetUint64());
    else
        return false;
    return true;
}

bool fromJson(const rapidjson::Value &v, float &dst)
{
    return getJsonNumber(v, dst);
}

bool fromJson(const rapidjson::Value &v, double &dst)
{
    return getJsonNumber(v, dst);
}

bool fromJson(const rapidjson::Value &v, uint32 &dst)
{
    return getJsonNumber(v, dst);
}

bool fromJson(const rapidjson::Value &v, int32 &dst)
{
    return getJsonNumber(v, dst);
}

bool fromJson(const rapidjson::Value &v, uint64 &dst)
{
    return getJsonNumber(v, dst);
}

bool fromJson(const rapidjson::Value &v, int64 &dst)
{
    return getJsonNumber(v, dst);
}

bool fromJson(const rapidjson::Value &v, std::string &dst)
{
    if (v.IsString()) {
        dst = std::move(std::string(v.GetString()));
        return true;
    }
    return false;
}

static Vec3f randomOrtho(const Vec3f &a)
{
    Vec3f res;
    if (std::abs(a.x()) > std::abs(a.y()))
        res = Vec3f(0.0f, 1.0f, 0.0f);
    else
        res = Vec3f(1.0f, 0.0f, 0.0f);
    return a.cross(res).normalized();
}

static void gramSchmidt(Vec3f &a, Vec3f &b, Vec3f &c)
{
    a.normalize();
    b -= a*a.dot(b);
    if (b.lengthSq() < 1e-5)
        b = randomOrtho(a);
    else
        b.normalize();

    c -= a*a.dot(c);
    c -= b*b.dot(c);
    if (c.lengthSq() < 1e-5)
        c = a.cross(b);
    else
        c.normalize();
}

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

static Vec3f prettifyVector(const Vec3f &p)
{
    return Vec3f(
        prettifyFloat(p.x()),
        prettifyFloat(p.y()),
        prettifyFloat(p.z())
    );
}

bool fromJson(const rapidjson::Value &v, Mat4f &dst)
{
    if (v.IsArray()) {
        ASSERT(v.Size() == 16, "Cannot convert Json Array to 4x4 Matrix: Invalid size");

        for (unsigned i = 0; i < 16; ++i)
            dst[i] = as<float>(v[i]);
        return true;
    } else if (v.IsObject()) {
        Vec3f x(1.0f, 0.0f, 0.0f);
        Vec3f y(0.0f, 1.0f, 0.0f);
        Vec3f z(0.0f, 0.0f, 1.0f);

        Vec3f pos(0.0f);
        fromJson(v, "position", pos);

        bool explicitX = false, explicitY = false, explicitZ = false;

        Vec3f lookAt;
        if (fromJson(v, "look_at", lookAt)) {
            z = lookAt - pos;
            explicitZ = true;
        }

        explicitY = fromJson(v, "up", y);

        explicitX = fromJson(v, "x_axis", x) || explicitX;
        explicitY = fromJson(v, "y_axis", y) || explicitY;
        explicitZ = fromJson(v, "z_axis", z) || explicitZ;

        int id =
            (explicitZ ? 4 : 0) +
            (explicitY ? 2 : 0) +
            (explicitX ? 1 : 0);
        switch (id) {
        case 0: gramSchmidt(z, y, x); break;
        case 1: gramSchmidt(x, z, y); break;
        case 2: gramSchmidt(y, z, x); break;
        case 3: gramSchmidt(y, x, z); break;
        case 4: gramSchmidt(z, y, x); break;
        case 5: gramSchmidt(z, x, y); break;
        case 6: gramSchmidt(z, y, x); break;
        case 7: gramSchmidt(z, y, x); break;
        }

        if (x.cross(y).dot(z) < 0.0f) {
            if (!explicitX)
                x = -x;
            else if (!explicitY)
                y = -y;
            else
                z = -z;
        }

        Vec3f scale;
        if (fromJson(v, "scale", scale)) {
            x *= scale.x();
            y *= scale.y();
            z *= scale.z();
        }

        Vec3f rot;
        if (fromJson(v, "rotation", rot)) {
            Mat4f tform = Mat4f::rotYXZ(rot);
            x = tform*x;
            y = tform*y;
            z = tform*z;
        }

        dst = Mat4f(
            x[0], y[0], z[0], pos[0],
            x[1], y[1], z[1], pos[1],
            x[2], y[2], z[2], pos[2],
            0.0f, 0.0f, 0.0f,   1.0f
        );

        return true;
    }
    return false;
}

bool fromJson(const rapidjson::Value &v, Path &dst)
{
    std::string path;
    bool result = fromJson(v, path);

    dst = Path(path);
    dst.freezeWorkingDirectory();

    return result;
}

rapidjson::Value toJsonValue(double value, rapidjson::Document::AllocatorType &/*allocator*/)
{
    return std::move(rapidjson::Value(value));
}

rapidjson::Value toJsonValue(const Mat4f &value, rapidjson::Document::AllocatorType &allocator)
{
    Vec3f   rot = prettifyVector(value.extractRotationVec());
    Vec3f scale = prettifyVector(value.extractScaleVec());
    Vec3f   pos = prettifyVector(value.extractTranslationVec());

    rapidjson::Value a(rapidjson::kObjectType);
    if (pos != 0.0f)
        a.AddMember("position", toJsonValue(pos, allocator), allocator);
    if (scale != 1.0f)
        a.AddMember("scale", toJsonValue(scale, allocator), allocator);
    if (rot != 0.0f)
        a.AddMember("rotation", toJsonValue(rot, allocator), allocator);

    return std::move(a);
}

void addObjectMember(rapidjson::Value &v, const char *name, const JsonSerializable &o,
        rapidjson::Document::AllocatorType &allocator)
{
    if (o.unnamed())
        v.AddMember(name, o.toJson(allocator), allocator);
    else
        v.AddMember(name, o.name().c_str(), allocator);
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
