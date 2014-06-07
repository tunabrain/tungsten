#ifndef MAT4F_HPP_
#define MAT4F_HPP_

#include "Vec.hpp"

namespace Tungsten
{

class Mat4f {
    union {
        struct {
            float a11, a12, a13, a14;
            float a21, a22, a23, a24;
            float a31, a32, a33, a34;
            float a41, a42, a43, a44;
        };
        float a[16];
    };

public:
    Mat4f transpose() const;
    Mat4f pseudoInvert() const;

    Mat4f();
    Mat4f(const Vec3f &right, const Vec3f &up, const Vec3f &fwd);
    Mat4f(float _a11, float _a12, float _a13, float _a14,
          float _a21, float _a22, float _a23, float _a24,
          float _a31, float _a32, float _a33, float _a34,
          float _a41, float _a42, float _a43, float _a44);

    Vec3f right() const
    {
        return Vec3f(a11, a21, a31);
    }

    Vec3f up() const
    {
        return Vec3f(a12, a22, a32);
    }

    Vec3f fwd() const
    {
        return Vec3f(a13, a23, a33);
    }

    float operator[](int i) const
    {
        return a[i];
    }

    float &operator[](int i)
    {
        return a[i];
    }

    const float *data() const
    {
        return a;
    }

    Vec3f transformVector(const Vec3f &b) const;

    Mat4f toNormalMatrix() const;

    Mat4f extractRotation() const;
    Mat4f extractTranslation() const;
    Vec3f extractScaleVec() const;
    Mat4f extractScale() const;
    Mat4f stripRotation() const;
    Mat4f stripTranslation() const;
    Mat4f stripScale() const;

    static Mat4f translate(const Vec3f &v);
    static Mat4f scale(const Vec3f &s);
    static Mat4f rotXYZ(const Vec3f &rot);
    static Mat4f rotYZX(const Vec3f &rot);
    static Mat4f rotAxis(const Vec3f &axis, float angle);

    static Mat4f ortho(float l, float r, float b, float t, float near, float far);
    static Mat4f perspective(float aov, float ratio, float near, float far);
    static Mat4f lookAt(const Vec3f &pos, const Vec3f &fwd, const Vec3f &up);
};

Mat4f operator*(const Mat4f &a, const Mat4f &b);
Vec4f operator*(const Mat4f &a, const Vec4f &b);
Vec3f operator*(const Mat4f &a, const Vec3f &b);

}

#endif
