#include "JsonPtr.hpp"

#include "JsonDocument.hpp"
#include "Path.hpp"

namespace Tungsten {

void JsonPtr::get(bool &dst) const
{
    if (_value->IsBool())
        dst = _value->GetBool();
    else
        parseError("Parameter has wrong type: Expecting a boolean value here");
}

template<typename T>
void getJsonNumber(const rapidjson::Value &v, T &dst, JsonPtr source)
{
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
        source.parseError("Parameter has wrong type: Expecting a number here");
}

void JsonPtr::get(float &dst) const
{
    getJsonNumber(*_value, dst, *this);
}

void JsonPtr::get(double &dst) const
{
    getJsonNumber(*_value, dst, *this);
}

void JsonPtr::get(uint8 &dst) const
{
    getJsonNumber(*_value, dst, *this);
}

void JsonPtr::get(uint32 &dst) const
{
    getJsonNumber(*_value, dst, *this);
}

void JsonPtr::get(int32 &dst) const
{
    getJsonNumber(*_value, dst, *this);
}

void JsonPtr::get(uint64 &dst) const
{
    getJsonNumber(*_value, dst, *this);
}

void JsonPtr::get(int64 &dst) const
{
    getJsonNumber(*_value, dst, *this);
}

void JsonPtr::get(std::string &dst) const
{
    dst = cast<const char *>();
}

void JsonPtr::get(const char *&dst) const
{
    if (isString())
        dst = _value->GetString();
    else
        parseError("Parameter has wrong type: Expecting a string value here");
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

void JsonPtr::get(Mat4f &dst) const
{
    if (isArray()) {
        if (size() != 16)
            parseError(tfm::format("Trying to parse a matrix, but this array has the wrong size "
                "(need 16 elements, received %d)", size()));
        for (unsigned i = 0; i < 16; ++i)
            dst[i] = operator[](i).cast<float>();
    } else if (isObject()) {
        Vec3f x(1.0f, 0.0f, 0.0f);
        Vec3f y(0.0f, 1.0f, 0.0f);
        Vec3f z(0.0f, 0.0f, 1.0f);

        Vec3f pos(0.0f);
        getField("position", pos);

        bool explicitX = false, explicitY = false, explicitZ = false;

        Vec3f lookAt;
        if (getField("look_at", lookAt)) {
            z = lookAt - pos;
            explicitZ = true;
        }

        explicitY = getField("up", y);

        explicitX = getField("x_axis", x) || explicitX;
        explicitY = getField("y_axis", y) || explicitY;
        explicitZ = getField("z_axis", z) || explicitZ;

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
        if (getField("scale", scale)) {
            x *= scale.x();
            y *= scale.y();
            z *= scale.z();
        }

        Vec3f rot;
        if (getField("rotation", rot)) {
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
    } else {
        parseError("Parameter has wrong type: Expecting a matrix value here");
    }
}

void JsonPtr::get(Path &dst) const
{
    dst = Path(cast<std::string>());
    dst.freezeWorkingDirectory();
}

void JsonPtr::parseError(std::string description) const
{
    _document->parseError(*this, std::move(description));
}

JsonMemberIterator JsonPtr::begin() const { return JsonMemberIterator(*this, _value->MemberBegin()); }
JsonMemberIterator JsonPtr::  end() const { return JsonMemberIterator(*this, _value->MemberEnd()); }

}
