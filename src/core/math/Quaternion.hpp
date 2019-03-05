#ifndef QUATERNION_HPP_
#define QUATERNION_HPP_

#include "Vec.hpp"
#include "Mat4f.hpp"
#include "Angle.hpp"

#include <cmath>

namespace Tungsten {

template<typename Type>
class Quaternion : public Vec<Type, 4>
{
    using Vec<Type, 4>::_v;

public:
    Quaternion() = default;

    inline Quaternion(const Vec<Type, 4> &a) : Vec<Type, 4>(a) {}

    inline explicit Quaternion(const Type &a)
    : Vec<Type, 4>(a)
    {
    }

    inline Quaternion(const Type &a, const Type &b, const Type &c, const Type &d)
    : Vec<Type, 4>(a, b, c, d)
    {
    }

    inline Quaternion(const Type &theta, const Vec<Type, 3> &u)
    {
        Type cosTheta = std::cos(theta/Type(2));
        Type sinTheta = std::sin(theta/Type(2));
        _v[0] = cosTheta;
        _v[1] = u.x()*sinTheta;
        _v[2] = u.y()*sinTheta;
        _v[3] = u.z()*sinTheta;
    }

    inline Quaternion conjugate() const
    {
        return Quaternion(_v[0], -_v[1], -_v[2], -_v[3]);
    }

    inline Quaternion slerp(Quaternion o, float t) const
    {
        double d = (*this).dot(o);
        if (d < 0.0f) {
            o = -o;
            d = -d;
        }
        if (d > 0.9995)
            return lerp(*this, o, t).normalized();

        float theta0 = std::acos(d);
        float theta = theta0*t;
        float sinTheta = std::sin(theta);
        float sinTheta0 = std::sin(theta0);

        float  s0 = std::cos(theta) - d*sinTheta/sinTheta0;
        float  s1 = sinTheta/sinTheta0;

        return (s0*(*this)) + (s1*o);
    }

    inline Quaternion operator*(const Quaternion &o)
    {
        return Quaternion(
            _v[0]*o[0] - _v[1]*o[1] - _v[2]*o[2] - _v[3]*o[3],
            _v[0]*o[1] + _v[1]*o[0] + _v[2]*o[3] - _v[3]*o[2],
            _v[0]*o[2] - _v[1]*o[3] + _v[2]*o[0] + _v[3]*o[1],
            _v[0]*o[3] + _v[1]*o[2] - _v[2]*o[1] + _v[3]*o[0]
        );
    }

    inline Vec<Type, 3> operator*(const Vec<Type, 3> &o) const
    {
        Type tx(Type(2)*(_v[2]*o[2] - _v[3]*o[1]));
        Type ty(Type(2)*(_v[3]*o[0] - _v[1]*o[2]));
        Type tz(Type(2)*(_v[1]*o[1] - _v[2]*o[0]));
        return Vec<Type, 3>(
            o[0] + _v[0]*tx + _v[2]*tz - _v[3]*ty,
            o[1] + _v[0]*ty + _v[3]*tx - _v[1]*tz,
            o[2] + _v[0]*tz + _v[1]*ty - _v[2]*tx
        );
    }

    inline Mat4f toMatrix() const
    {
        const Type one(1), two(2);
        Type w(_v[0]), x(_v[1]), y(_v[2]), z(_v[3]);
        return Mat4f(
            one - two*y*y - two*z*z,       two*x*y - two*w*z,       two*x*z + two*w*y, 0.0f,
                  two*x*y + two*w*z, one - two*x*x - two*z*z,       two*y*z - two*w*x, 0.0f,
                  two*x*z - two*w*y,       two*y*z + two*w*x, one - two*x*x - two*y*y, 0.0f,
                               0.0f,                    0.0f,                    0.0f, 1.0f
        );
    }

    inline Vec3f toEuler() const
    {
        return Vec3f(
            Angle::radToDeg(std::atan2(2.0f*(_v[0]*_v[1] + _v[2]*_v[3]), 1.0f - 2.0f*(_v[1]*_v[1] + _v[2]*_v[2]))),
            Angle::radToDeg(std::asin(2.0f*(_v[0]*_v[2] - _v[3]*_v[1]))),
            Angle::radToDeg(std::atan2(2.0f*(_v[0]*_v[3] + _v[1]*_v[2]), 1.0f - 2.0f*(_v[2]*_v[2] + _v[3]*_v[3])))
        );
    }

    static inline Quaternion<Type> fromMatrix(const Mat4f &a) {
        float trace = a(0, 0) + a(1, 1) + a(2, 2);
        if (trace > 0.0f) {
            float s = 0.5f/std::sqrt(trace + 1.0f);
            return Quaternion<Type>(
                0.25f/s,
                (a(2, 1) - a(1, 2))*s,
                (a(0, 2) - a(2, 0))*s,
                (a(1, 0) - a(0, 1))*s
            );
        } else if (a(0, 0) > a(1, 1) && a(0, 0) > a(2, 2)) {
            float s = 2.0f*std::sqrt(1.0f + a(0, 0) - a(1, 1) - a(2, 2));
            return Quaternion<Type>(
                (a(2, 1) - a(1, 2))/s,
                0.25f*s,
                (a(0, 1) + a(1, 0))/s,
                (a(0, 2) + a(2, 0))/s
            );
        } else if (a(1, 1) > a(2, 2)) {
            float s = 2.0f*std::sqrt(1.0f + a(1, 1) - a(0, 0) - a(2, 2));
            return Quaternion<Type>(
                (a(0, 2) - a(2, 0))/s,
                (a(0, 1) + a(1, 0))/s,
                0.25f*s,
                (a(1, 2) + a(2, 1))/s
            );
        } else {
            float s = 2.0f*std::sqrt(1.0f + a(2, 2) - a(0, 0) - a(1, 1));
            return Quaternion<Type>(
                (a(1, 0) - a(0, 1))/s,
                (a(0, 2) + a(2, 0))/s,
                (a(1, 2) + a(2, 1))/s,
                0.25f*s
            );
        }
    }

    static inline Quaternion<Type> identity()
    {
        return Quaternion(1.0f, 0.0f, 0.0f, 0.0f);
    }

    static inline Quaternion<Type> lookAt(const Vec<Type, 3> &a, const Vec<Type, 3> &b)
    {
        Vec3f forward = (a - b).normalized();
        if (std::abs(forward.y()) > 1.0f - 1e-3f)
            return Quaternion(forward.y() < 0.0f ? PI : 0.0f, Vec3f(0.0f, 1.0f, 0.0f));

        Vec3f axis = Vec3f(forward.z(), 0.0f, -forward.x());
        return Quaternion(std::acos(forward.y()), axis.normalized());
    }
};

template<typename Type>
inline Quaternion<Type> operator*(const Vec<Type, 3> &a, const Quaternion<Type> &o)
{
    return Quaternion<Type>(
        -a[0]*o[1] - a[1]*o[2] - a[2]*o[3],
         a[0]*o[0] + a[1]*o[3] - a[2]*o[2],
        -a[0]*o[3] + a[1]*o[0] + a[2]*o[1],
         a[0]*o[2] - a[1]*o[1] + a[2]*o[0]
    );
}

typedef Quaternion<float> QuaternionF;

};

#endif /* QUATERNION_HPP_ */
