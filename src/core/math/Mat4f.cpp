#include "Mat4f.hpp"

#include "Quaternion.hpp"
#include "Angle.hpp"

#include <cmath>

namespace Tungsten {

Mat4f Mat4f::toNormalMatrix() const
{
    return scale(1.0f/Vec3f(right().lengthSq(), up().lengthSq(), fwd().lengthSq()))**this;
}

Vec3f Mat4f::extractRotationVec() const
{
    Mat4f m = extractRotation();
    float theta, phi, psi;
    if (m.a23 <= -1.0f) {
        theta = PI_HALF;
        phi = std::atan2(m.a31, m.a32);
        psi = 0.0f;
    } else if (m.a23 >= 1.0f) {
        theta = -PI_HALF;
        phi = std::atan2(-m.a31, -m.a32);
        psi = 0.0f;
    } else {
        theta = std::asin(m.a23);
        phi = std::atan2(m.a21, m.a22);
        psi = std::atan2(m.a13, m.a33);
    }
    Vec3f v = Vec3f(
        Angle::radToDeg(-theta),
        Angle::radToDeg(-psi),
        Angle::radToDeg(phi)
    );
    return v;
}

Mat4f Mat4f::extractRotation() const
{
    return Mat4f(
        right().normalized(),
        up().normalized(),
        fwd().normalized()
    );
}

Vec3f Mat4f::extractScaleVec() const
{
    return Vec3f(right().length(), up().length(), fwd().length());
}

Mat4f Mat4f::extractScale() const
{
    return scale(Vec3f(right().length(), up().length(), fwd().length()));
}

Vec3f Mat4f::extractTranslationVec() const
{
    return Vec3f(a14, a24, a34);
}

Mat4f Mat4f::extractTranslation() const
{
    return translate(Vec3f(a14, a24, a34));
}

Mat4f Mat4f::stripRotation() const
{
    return extractTranslation()*extractScale();
}

Mat4f Mat4f::stripScale() const
{
    return extractTranslation()*extractRotation();
}

Mat4f Mat4f::stripTranslation() const
{
    return extractRotation()*extractScale();
}

Mat4f Mat4f::translate(const Vec3f &v)
{
    return Mat4f(
        1.0f, 0.0f, 0.0f, v.x(),
        0.0f, 1.0f, 0.0f, v.y(),
        0.0f, 0.0f, 1.0f, v.z(),
        0.0f, 0.0f, 0.0f, 1.0f
    );
}

Mat4f Mat4f::scale(const Vec3f &s)
{
    return Mat4f(
        s.x(),  0.0f,  0.0f, 0.0f,
         0.0f, s.y(),  0.0f, 0.0f,
         0.0f,  0.0f, s.z(), 0.0f,
         0.0f,  0.0f,  0.0f, 1.0f
    );
}

Mat4f Mat4f::rotXYZ(const Vec3f &rot)
{
    Vec3f r = rot*PI/180.0f;
    float c[] = {std::cos(r.x()), std::cos(r.y()), std::cos(r.z())};
    float s[] = {std::sin(r.x()), std::sin(r.y()), std::sin(r.z())};

    return Mat4f(
        c[1]*c[2], -c[0]*s[2] + s[0]*s[1]*c[2],  s[0]*s[2] + c[0]*s[1]*c[2], 0.0f,
        c[1]*s[2],  c[0]*c[2] + s[0]*s[1]*s[2], -s[0]*c[2] + c[0]*s[1]*s[2], 0.0f,
            -s[1],                   s[0]*c[1],                   c[0]*c[1], 0.0f,
             0.0f,                        0.0f,                        0.0f, 1.0f
    );
}

Mat4f Mat4f::rotYXZ(const Vec3f &rot)
{
    Vec3f r = rot*PI/180.0f;
    float c[] = {std::cos(r.x()), std::cos(r.y()), std::cos(r.z())};
    float s[] = {std::sin(r.x()), std::sin(r.y()), std::sin(r.z())};

    return Mat4f(
        c[1]*c[2] - s[1]*s[0]*s[2],   -c[1]*s[2] - s[1]*s[0]*c[2], -s[1]*c[0], 0.0f,
                         c[0]*s[2],                     c[0]*c[2],      -s[0], 0.0f,
        s[1]*c[2] + c[1]*s[0]*s[2],   -s[1]*s[2] + c[1]*s[0]*c[2],  c[1]*c[0], 0.0f,
                              0.0f,                          0.0f,       0.0f, 1.0f
    );
}

Mat4f Mat4f::rotAxis(const Vec3f &axis, float angle)
{
    angle = Angle::degToRad(angle);
    float s = std::sin(angle);
    float c = std::cos(angle);
    float c1 = 1.0f - c;
    float x = axis.x();
    float y = axis.y();
    float z = axis.z();

    return Mat4f(
           c + x*x*c1,  x*y*c1 - z*s,  x*z*c1 + y*s, 0.0f,
         y*x*c1 + z*s,    c + y*y*c1,  y*z*c1 - x*s, 0.0f,
         z*x*c1 - y*s,  z*y*c1 + x*s,    c + z*z*c1, 0.0f,
                 0.0f,          0.0f,          0.0f, 1.0f
    );
}

Mat4f Mat4f::ortho(float l, float r, float b, float t, float n, float f)
{
    return Mat4f(
        2.0f/(r-l),       0.0f,        0.0f, -(r+l)/(r-l),
              0.0f, 2.0f/(t-b),        0.0f, -(t+b)/(t-b),
              0.0f,       0.0f, -2.0f/(f-n), -(f+n)/(f-n),
              0.0f,       0.0f,        0.0f,          1.0f
    );
}

Mat4f Mat4f::perspective(float fov, float ratio, float near, float far)
{
    float t = 1.0f/std::tan(Angle::degToRad(fov)*0.5f);
    float a = (far + near)/(far - near);
    float b = 2.0f*far*near/(far - near);
    float c = t/ratio;

    return Mat4f(
           c, 0.0f,  0.0f, 0.0f,
        0.0f,    t,  0.0f, 0.0f,
        0.0f, 0.0f,     a,   -b,
        0.0f, 0.0f,  1.0f, 0.0f
    );
}

Mat4f Mat4f::lookAt(const Vec3f &pos, const Vec3f &fwd, const Vec3f &up)
{
    Vec3f f = fwd.normalized();
    Vec3f r = f.cross(up).normalized();
    Vec3f u = r.cross(f).normalized();

    return Mat4f(
        r.x(), u.x(), f.x(), pos.x(),
        r.y(), u.y(), f.y(), pos.y(),
        r.z(), u.z(), f.z(), pos.z(),
         0.0f,  0.0f,  0.0f,    1.0f
    );

}

}
