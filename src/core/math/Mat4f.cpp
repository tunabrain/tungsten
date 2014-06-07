#include <cmath>

#include "Mat4f.hpp"
#include "Angle.hpp"

namespace Tungsten
{

Mat4f::Mat4f() {
    a12 = a13 = a14 = 0.0f;
    a21 = a23 = a24 = 0.0f;
    a31 = a32 = a34 = 0.0f;
    a41 = a42 = a43 = 0.0f;
    a11 = a22 = a33 = a44 = 1.0f;
}

Mat4f::Mat4f(const Vec3f &x, const Vec3f &y, const Vec3f &z)
: a11(x.x()), a12(y.x()), a13(z.x()), a14( 0.0f),
  a21(x.y()), a22(y.y()), a23(z.y()), a24( 0.0f),
  a31(x.z()), a32(y.z()), a33(z.z()), a34( 0.0f),
  a41( 0.0f), a42( 0.0f), a43( 0.0f), a44( 1.0f)
{
}

Mat4f::Mat4f(
    float _a11, float _a12, float _a13, float _a14,
    float _a21, float _a22, float _a23, float _a24,
    float _a31, float _a32, float _a33, float _a34,
    float _a41, float _a42, float _a43, float _a44)
: a11(_a11), a12(_a12), a13(_a13), a14(_a14),
  a21(_a21), a22(_a22), a23(_a23), a24(_a24),
  a31(_a31), a32(_a32), a33(_a33), a34(_a34),
  a41(_a41), a42(_a42), a43(_a43), a44(_a44)
{
}

Mat4f Mat4f::transpose() const {
    return Mat4f(
        a11, a21, a31, a41,
        a12, a22, a32, a42,
        a13, a23, a33, a43,
        a14, a24, a34, a44
    );
}

Mat4f Mat4f::pseudoInvert() const {
    Mat4f trans = translate(Vec3f(-a14, -a24, -a34));
    Mat4f rot = transpose();
    rot.a41 = rot.a42 = rot.a43 = 0.0f;

    return rot*trans;
}

Vec3f Mat4f::transformVector(const Vec3f &b) const
{
    return Vec3f(
        a11*b.x() + a12*b.y() + a13*b.z(),
        a21*b.x() + a22*b.y() + a23*b.z(),
        a31*b.x() + a32*b.y() + a33*b.z()
    );
}

Mat4f Mat4f::toNormalMatrix() const
{
    return scale(1.0f/Vec3f(right().lengthSq(), up().lengthSq(), fwd().lengthSq()))**this;
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

Mat4f Mat4f::translate(const Vec3f &v) {
    return Mat4f(
        1.0f, 0.0f, 0.0f, v.x(),
        0.0f, 1.0f, 0.0f, v.y(),
        0.0f, 0.0f, 1.0f, v.z(),
        0.0f, 0.0f, 0.0f, 1.0f
    );
}

Mat4f Mat4f::scale(const Vec3f &s) {
    return Mat4f(
        s.x(),  0.0f,  0.0f, 0.0f,
         0.0f, s.y(),  0.0f, 0.0f,
         0.0f,  0.0f, s.z(), 0.0f,
         0.0f,  0.0f,  0.0f, 1.0f
    );
}

Mat4f Mat4f::rotXYZ(const Vec3f &rot) {
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

Mat4f Mat4f::rotYZX(const Vec3f &rot) {
    Vec3f r = rot*PI/180.0f;
    float c[] = {std::cos(r.x()), std::cos(r.y()), std::cos(r.z())};
    float s[] = {std::sin(r.x()), std::sin(r.y()), std::sin(r.z())};

    return Mat4f(
         c[1]*c[2],  c[0]*c[1]*s[2] - s[0]*s[1], c[0]*s[1] + c[1]*s[0]*s[2], 0.0f,
             -s[2],                   c[0]*c[2],                  c[2]*s[0], 0.0f,
        -c[2]*s[1], -c[1]*s[0] - c[0]*s[1]*s[2], c[0]*c[1] - s[0]*s[1]*s[2], 0.0f,
              0.0f,                        0.0f,                       0.0f, 1.0f
    );
}

Mat4f Mat4f::rotAxis(const Vec3f &axis, float angle) {
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

Mat4f Mat4f::ortho(float l, float r, float b, float t, float n, float f) {
    return Mat4f(
        2.0f/(r-l),       0.0f,        0.0f, -(r+l)/(r-l),
              0.0f, 2.0f/(t-b),        0.0f, -(t+b)/(t-b),
              0.0f,       0.0f, -2.0f/(f-n), -(f+n)/(f-n),
              0.0f,       0.0f,        0.0f,          1.0f
    );
}

Mat4f Mat4f::perspective(float fov, float ratio, float near, float far) {
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

Mat4f Mat4f::lookAt(const Vec3f &pos, const Vec3f &fwd, const Vec3f &up) {
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

Mat4f operator*(const Mat4f &a, const Mat4f &b) {
    Mat4f result;
    for (int i = 0; i < 4; i++)
        for (int t = 0; t < 4; t++)
            result[i*4 + t] =
                a[i*4 + 0]*b[0*4 + t] +
                a[i*4 + 1]*b[1*4 + t] +
                a[i*4 + 2]*b[2*4 + t] +
                a[i*4 + 3]*b[3*4 + t];

    return result;
}

Vec4f operator*(const Mat4f &a, const Vec4f &b) {
    return Vec4f(
        a.a11*b.x() + a.a12*b.y() + a.a13*b.z() + a.a14*b.w(),
        a.a21*b.x() + a.a22*b.y() + a.a23*b.z() + a.a24*b.w(),
        a.a31*b.x() + a.a32*b.y() + a.a33*b.z() + a.a34*b.w(),
        a.a41*b.x() + a.a42*b.y() + a.a43*b.z() + a.a44*b.w()
    );
}

Vec3f operator*(const Mat4f &a, const Vec3f &b) {
    return Vec3f(
        a.a11*b.x() + a.a12*b.y() + a.a13*b.z() + a.a14,
        a.a21*b.x() + a.a22*b.y() + a.a23*b.z() + a.a24,
        a.a31*b.x() + a.a32*b.y() + a.a33*b.z() + a.a34
    );
}

}
