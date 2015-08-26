#ifndef POLYNOMIAL_HPP_
#define POLYNOMIAL_HPP_

namespace Tungsten {

namespace Polynomial {

// Computes polynomial P(x)
template<int Size, typename T>
static inline double eval(T x, const T P[])
{
    T result = P[Size - 1];
    for (int i = Size - 2; i >= 0; --i) {
       result *= x;
       result += P[i];
    }
    return result;
}

// Computes rational polynomial P(x)/Q(x)
template<int Size>
static inline double rational(double x, const double P[], const double Q[])
{
    if (x <= 1) {
        double s1 = P[Size - 1];
        double s2 = Q[Size - 1];
        for (int i = Size - 2; i >= 0; --i) {
            s1 *= x;
            s2 *= x;
            s1 += P[i];
            s2 += Q[i];
        }
        return s1/s2;
    } else {
        x = 1.0f/x;
        double s1 = P[0];
        double s2 = Q[0];
        for (int i = 1; i < Size; ++i) {
            s1 *= x;
            s2 *= x;
            s1 += P[i];
            s2 += Q[i];
        }
        return s1/s2;
    }
}

}

}

#endif /* POLYNOMIAL_HPP_ */
