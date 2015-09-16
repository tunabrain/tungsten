#ifndef MAT4F_HPP_
#define MAT4F_HPP_

#include "Vec.hpp"

namespace Tungsten {

class Mat4f;

static inline Mat4f operator*(const Mat4f &a, const Mat4f &b);
static inline Vec4f operator*(const Mat4f &a, const Vec4f &b);
static inline Vec3f operator*(const Mat4f &a, const Vec3f &b);

class Mat4f
{
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
    Mat4f()
    {
        a12 = a13 = a14 = 0.0f;
        a21 = a23 = a24 = 0.0f;
        a31 = a32 = a34 = 0.0f;
        a41 = a42 = a43 = 0.0f;
        a11 = a22 = a33 = a44 = 1.0f;
    }

    Mat4f(const Vec3f &right, const Vec3f &up, const Vec3f &fwd)
    : a11(right.x()), a12(up.x()), a13(fwd.x()), a14(0.0f),
      a21(right.y()), a22(up.y()), a23(fwd.y()), a24(0.0f),
      a31(right.z()), a32(up.z()), a33(fwd.z()), a34(0.0f),
      a41(0.0f), a42(0.0f), a43(0.0f), a44(1.0f)
    {
    }

    Mat4f(
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

    Mat4f transpose() const
    {
        return Mat4f(
            a11, a21, a31, a41,
            a12, a22, a32, a42,
            a13, a23, a33, a43,
            a14, a24, a34, a44
        );
    }

    Mat4f pseudoInvert() const
    {
        Mat4f trans = translate(Vec3f(-a14, -a24, -a34));
        Mat4f rot = transpose();
        rot.a41 = rot.a42 = rot.a43 = 0.0f;

        return rot*trans;
    }

    Mat4f invert() const
    {
        Mat4f inv;
        inv[ 0] =  a[5]*a[10]*a[15] - a[5]*a[11]*a[14] - a[9]*a[6]*a[15] + a[9]*a[7]*a[14] + a[13]*a[6]*a[11] - a[13]*a[7]*a[10];
        inv[ 1] = -a[1]*a[10]*a[15] + a[1]*a[11]*a[14] + a[9]*a[2]*a[15] - a[9]*a[3]*a[14] - a[13]*a[2]*a[11] + a[13]*a[3]*a[10];
        inv[ 2] =  a[1]*a[ 6]*a[15] - a[1]*a[ 7]*a[14] - a[5]*a[2]*a[15] + a[5]*a[3]*a[14] + a[13]*a[2]*a[ 7] - a[13]*a[3]*a[ 6];
        inv[ 3] = -a[1]*a[ 6]*a[11] + a[1]*a[ 7]*a[10] + a[5]*a[2]*a[11] - a[5]*a[3]*a[10] - a[ 9]*a[2]*a[ 7] + a[ 9]*a[3]*a[ 6];
        inv[ 4] = -a[4]*a[10]*a[15] + a[4]*a[11]*a[14] + a[8]*a[6]*a[15] - a[8]*a[7]*a[14] - a[12]*a[6]*a[11] + a[12]*a[7]*a[10];
        inv[ 5] =  a[0]*a[10]*a[15] - a[0]*a[11]*a[14] - a[8]*a[2]*a[15] + a[8]*a[3]*a[14] + a[12]*a[2]*a[11] - a[12]*a[3]*a[10];
        inv[ 6] = -a[0]*a[ 6]*a[15] + a[0]*a[ 7]*a[14] + a[4]*a[2]*a[15] - a[4]*a[3]*a[14] - a[12]*a[2]*a[ 7] + a[12]*a[3]*a[ 6];
        inv[ 8] =  a[4]*a[ 9]*a[15] - a[4]*a[11]*a[13] - a[8]*a[5]*a[15] + a[8]*a[7]*a[13] + a[12]*a[5]*a[11] - a[12]*a[7]*a[ 9];
        inv[ 7] =  a[0]*a[ 6]*a[11] - a[0]*a[ 7]*a[10] - a[4]*a[2]*a[11] + a[4]*a[3]*a[10] + a[ 8]*a[2]*a[ 7] - a[ 8]*a[3]*a[ 6];
        inv[ 9] = -a[0]*a[ 9]*a[15] + a[0]*a[11]*a[13] + a[8]*a[1]*a[15] - a[8]*a[3]*a[13] - a[12]*a[1]*a[11] + a[12]*a[3]*a[ 9];
        inv[10] =  a[0]*a[ 5]*a[15] - a[0]*a[ 7]*a[13] - a[4]*a[1]*a[15] + a[4]*a[3]*a[13] + a[12]*a[1]*a[ 7] - a[12]*a[3]*a[ 5];
        inv[11] = -a[0]*a[ 5]*a[11] + a[0]*a[ 7]*a[ 9] + a[4]*a[1]*a[11] - a[4]*a[3]*a[ 9] - a[ 8]*a[1]*a[ 7] + a[ 8]*a[3]*a[ 5];
        inv[12] = -a[4]*a[ 9]*a[14] + a[4]*a[10]*a[13] + a[8]*a[5]*a[14] - a[8]*a[6]*a[13] - a[12]*a[5]*a[10] + a[12]*a[6]*a[ 9];
        inv[13] =  a[0]*a[ 9]*a[14] - a[0]*a[10]*a[13] - a[8]*a[1]*a[14] + a[8]*a[2]*a[13] + a[12]*a[1]*a[10] - a[12]*a[2]*a[ 9];
        inv[14] = -a[0]*a[ 5]*a[14] + a[0]*a[ 6]*a[13] + a[4]*a[1]*a[14] - a[4]*a[2]*a[13] - a[12]*a[1]*a[ 6] + a[12]*a[2]*a[ 5];
        inv[15] =  a[0]*a[ 5]*a[10] - a[0]*a[ 6]*a[ 9] - a[4]*a[1]*a[10] + a[4]*a[2]*a[ 9] + a[ 8]*a[1]*a[ 6] - a[ 8]*a[2]*a[ 5];

        float det = a[0]*inv[0] + a[1]*inv[4] + a[2]*inv[8] + a[3]*inv[12];
        if (det == 0.0f)
            return Mat4f();

        float invDet = 1.0f/det;
        return inv*invDet;
    }

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

    void setRight(const Vec3f &x)
    {
        a11 = x.x();
        a21 = x.y();
        a31 = x.z();
    }

    void setUp(const Vec3f &y)
    {
        a12 = y.x();
        a22 = y.y();
        a32 = y.z();
    }

    void setFwd(const Vec3f &z)
    {
        a13 = z.x();
        a23 = z.y();
        a33 = z.z();
    }

    float operator()(int i, int j) const
    {
        return a[i*4 + j];
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

    Vec3f transformVector(const Vec3f &b) const
    {
        return Vec3f(
            a11*b.x() + a12*b.y() + a13*b.z(),
            a21*b.x() + a22*b.y() + a23*b.z(),
            a31*b.x() + a32*b.y() + a33*b.z()
        );
    }

    Mat4f operator-(const Mat4f &o) const
    {
        Mat4f tmp(*this);
        for (int i = 0; i < 16; ++i)
            tmp[i] -= o[i];
        return tmp;
    }

    Mat4f operator+(const Mat4f &o) const
    {
        Mat4f tmp(*this);
        for (int i = 0; i < 16; ++i)
            tmp[i] += o[i];
        return tmp;
    }

    Mat4f operator-(float o) const
    {
        Mat4f tmp(*this);
        for (int i = 0; i < 16; ++i)
            tmp[i] -= o;
        return tmp;
    }

    Mat4f operator+(float o) const
    {
        Mat4f tmp(*this);
        for (int i = 0; i < 16; ++i)
            tmp[i] += o;
        return tmp;
    }

    Mat4f &operator-=(const Mat4f &o)
    {
        for (int i = 0; i < 16; ++i)
            a[i] -= o[i];
        return *this;
    }

    Mat4f &operator+=(const Mat4f &o)
    {
        for (int i = 0; i < 16; ++i)
            a[i] += o[i];
        return *this;
    }

    Mat4f &operator-=(float o)
    {
        for (int i = 0; i < 16; ++i)
            a[i] -= o;
        return *this;
    }

    Mat4f &operator+=(float o)
    {
        for (int i = 0; i < 16; ++i)
            a[i] += o;
        return *this;
    }

    Mat4f operator*(float o) const
    {
        Mat4f tmp(*this);
        for (int i = 0; i < 16; ++i)
            tmp[i] *= o;
        return tmp;
    }

    Mat4f operator/(float o) const
    {
        Mat4f tmp(*this);
        for (int i = 0; i < 16; ++i)
            tmp[i] /= o;
        return tmp;
    }

    Mat4f &operator*=(float o)
    {
        for (int i = 0; i < 16; ++i)
            a[i] *= o;
        return *this;
    }

    Mat4f &operator/=(float o)
    {
        for (int i = 0; i < 16; ++i)
            a[i] /= o;
        return *this;
    }

    Mat4f toNormalMatrix() const;

    Vec3f extractRotationVec() const;
    Mat4f extractRotation() const;
    Vec3f extractTranslationVec() const;
    Mat4f extractTranslation() const;
    Vec3f extractScaleVec() const;
    Mat4f extractScale() const;
    Mat4f stripRotation() const;
    Mat4f stripTranslation() const;
    Mat4f stripScale() const;

    static Mat4f translate(const Vec3f &v);
    static Mat4f scale(const Vec3f &s);
    static Mat4f rotXYZ(const Vec3f &rot);
    static Mat4f rotYXZ(const Vec3f &rot);
    static Mat4f rotAxis(const Vec3f &axis, float angle);

    static Mat4f ortho(float l, float r, float b, float t, float near, float far);
    static Mat4f perspective(float aov, float ratio, float near, float far);
    static Mat4f lookAt(const Vec3f &pos, const Vec3f &fwd, const Vec3f &up);
    
    friend Mat4f operator*(const Mat4f &a, const Mat4f &b);
    friend Vec4f operator*(const Mat4f &a, const Vec4f &b);
    friend Vec3f operator*(const Mat4f &a, const Vec3f &b);

    friend std::ostream &operator<< (std::ostream &stream, const Mat4f &m) {
        for (int i = 0; i < 4; ++i) {
            stream << '[';
            for (uint32 j = 0; j < 4; ++j)
                stream << m[i*4 + j] << (j == 3 ? ']' : ',');
            if (i < 3)
                stream << std::endl;
        }
        return stream;
    }
};

static inline Mat4f operator*(const Mat4f &a, const Mat4f &b)
{
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

static inline Vec4f operator*(const Mat4f &a, const Vec4f &b)
{
    return Vec4f(
        a.a11*b.x() + a.a12*b.y() + a.a13*b.z() + a.a14*b.w(),
        a.a21*b.x() + a.a22*b.y() + a.a23*b.z() + a.a24*b.w(),
        a.a31*b.x() + a.a32*b.y() + a.a33*b.z() + a.a34*b.w(),
        a.a41*b.x() + a.a42*b.y() + a.a43*b.z() + a.a44*b.w()
    );
}

static inline Vec3f operator*(const Mat4f &a, const Vec3f &b)
{
    return Vec3f(
        a.a11*b.x() + a.a12*b.y() + a.a13*b.z() + a.a14,
        a.a21*b.x() + a.a22*b.y() + a.a23*b.z() + a.a24,
        a.a31*b.x() + a.a32*b.y() + a.a33*b.z() + a.a34
    );
}

}

#endif
