#ifndef FMATH_HPP_
#define FMATH_HPP_ 1

/**
	@brief fast math library for float
	@author herumi
	@url https://github.com/herumi/fmath/
	@note modified new BSD license
	http://opensource.org/licenses/BSD-3-Clause

	cl /Ox /Ob2 /arch:SSE2 /fp:fast bench.cpp -I../xbyak /EHsc /DNOMINMAX
	g++ -O3 -fomit-frame-pointer -fno-operator-names -march=core2 -mssse3 -mfpmath=sse -ffast-math -fexcess-precision=fast
*/

#include <math.h>
#include <stddef.h>
#include <assert.h>
#include <limits>
#include <stdlib.h>
#include <float.h>
#include <string.h> // for memcpy
#if defined(_WIN32) && !defined(__GNUC__)
	#include <intrin.h>
	#ifndef MIE_ALIGN
		#define MIE_ALIGN(x) __declspec(align(x))
	#endif
#else
	#ifndef __GNUC_PREREQ
	#define __GNUC_PREREQ(major, minor) ((((__GNUC__) << 16) + (__GNUC_MINOR__)) >= (((major) << 16) + (minor)))
	#endif
	#if __GNUC_PREREQ(4, 4) || !defined(__GNUC__)
		/* GCC >= 4.4 and non-GCC compilers */
		#include <x86intrin.h>
	#elif __GNUC_PREREQ(4, 1)
		/* GCC 4.1, 4.2, and 4.3 do not have x86intrin.h, directly include SSE2 header */
		#include <emmintrin.h>
	#endif
	#ifndef MIE_ALIGN
		#define MIE_ALIGN(x) __attribute__((aligned(x)))
	#endif
#endif
#ifndef MIE_PACK
	#define MIE_PACK(x, y, z, w) ((x) * 64 + (y) * 16 + (z) * 4 + (w))
#endif

namespace fmath {

namespace local {

const size_t EXP_TABLE_SIZE = 10;
const size_t EXPD_TABLE_SIZE = 11;
const size_t LOG_TABLE_SIZE = 12;

typedef unsigned long long uint64_t;

union fi {
	float f;
	unsigned int i;
};

union di {
	double d;
	uint64_t i;
};

inline unsigned int mask(int x)
{
	return (1U << x) - 1;
}

inline uint64_t mask64(int x)
{
	return (1ULL << x) - 1;
}

template<class T>
inline const T* cast_to(const void *p)
{
	return reinterpret_cast<const T*>(p);
}

template<class T, size_t N>
size_t NumOfArray(const T (&)[N]) { return N; }

/*
	exp(88.722839f) = inf ; 0x42b17218
	exp(-87.33655f) = 1.175491e-038f(007fffe6) denormal ; 0xc2aeac50
	exp(-103.972081f) = 0 ; 0xc2cff1b5
*/
template<size_t N = EXP_TABLE_SIZE>
struct ExpVar {
	enum {
		s = N,
		n = 1 << s,
		f88 = 0x42b00000 /* 88.0 */
	};
	float minX[8];
	float maxX[8];
	float a[8];
	float b[8];
	float f1[8];
	unsigned int i127s[8];
	unsigned int mask_s[8];
	unsigned int i7fffffff[8];
	unsigned int tbl[n];
	ExpVar()
	{
		float log_2 = ::logf(2.0f);
		for (int i = 0; i < 8; i++) {
			maxX[i] = 88;
			minX[i] = -88;
			a[i] = n / log_2;
			b[i] = log_2 / n;
			f1[i] = 1.0f;
			i127s[i] = 127 << s;
			i7fffffff[i] = 0x7fffffff;
			mask_s[i] = mask(s);
		}

		for (int i = 0; i < n; i++) {
			float y = pow(2.0f, (float)i / n);
			fi fi;
			fi.f = y;
			tbl[i] = fi.i & mask(23);
		}
	}
};

template<size_t sbit_ = EXPD_TABLE_SIZE>
struct ExpdVar {
	enum {
		sbit = sbit_,
		s = 1UL << sbit,
		adj = (1UL << (sbit + 10)) - (1UL << sbit)
	};
	// A = 1, B = 1, C = 1/2, D = 1/6
	double C1[2]; // A
	double C2[2]; // D
	double C3[2]; // C/D
	uint64_t tbl[s];
	double a;
	double ra;
	ExpdVar()
		: a(s / ::log(2.0))
		, ra(1 / a)
	{
		for (int i = 0; i < 2; i++) {
			C1[i] = 1.0;
			C2[i] = 0.16666666685227835064;
			C3[i] = 3.0000000027955394;
		}
		for (int i = 0; i < s; i++) {
			di di;
			di.d = ::pow(2.0, i * (1.0 / s));
			tbl[i] = di.i & mask64(52);
		}
	}
};

template<size_t N = LOG_TABLE_SIZE>
struct LogVar {
	enum {
		LEN = N - 1
	};
	unsigned int m1[4]; // 0
	unsigned int m2[4]; // 16
	unsigned int m3[4]; // 32
	float m4[4];		// 48
	unsigned int m5[4]; // 64
	struct {
		float app;
		float rev;
	} tbl[1 << LEN];
	float c_log2;
	LogVar()
		: c_log2(::logf(2.0f) / (1 << 23))
	{
		const double e = 1 / double(1 << 24);
		const double h = 1 / double(1 << LEN);
		const size_t n = 1U << LEN;
		for (size_t i = 0; i < n; i++) {
			double x = 1 + double(i) / n;
			double a = ::log(x);
			tbl[i].app = (float)a;
			if (i < n - 1) {
				double b = ::log(x + h - e);
				tbl[i].rev = (float)((b - a) / ((h - e) * (1 << 23)));
			} else {
				tbl[i].rev = (float)(1 / (x * (1 << 23)));
			}
		}
		for (int i = 0; i < 4; i++) {
			m1[i] = mask(8) << 23;
			m2[i] = mask(LEN) << (23 - LEN);
			m3[i] = mask(23 - LEN);
			m4[i] = c_log2;
			m5[i] = 127U << 23;
		}
	}
};

/* to define static variables in fmath.hpp */
template<size_t EXP_N = EXP_TABLE_SIZE, size_t LOG_N = LOG_TABLE_SIZE, size_t EXPD_N = EXPD_TABLE_SIZE>
struct C {
	static const ExpVar<EXP_N> expVar;
	static const LogVar<LOG_N> logVar;
	static const ExpdVar<EXPD_N> expdVar;
};

template<size_t EXP_N, size_t LOG_N, size_t EXPD_N>
MIE_ALIGN(32) const ExpVar<EXP_N> C<EXP_N, LOG_N, EXPD_N>::expVar;

template<size_t EXP_N, size_t LOG_N, size_t EXPD_N>
MIE_ALIGN(32) const LogVar<LOG_N> C<EXP_N, LOG_N, EXPD_N>::logVar;

template<size_t EXP_N, size_t LOG_N, size_t EXPD_N>
MIE_ALIGN(32) const ExpdVar<EXPD_N> C<EXP_N, LOG_N, EXPD_N>::expdVar;

} // fmath::local

inline float exp(float x)
{
	using namespace local;
	const ExpVar<>& expVar = C<>::expVar;

	__m128 x1 = _mm_set_ss(x);

	int limit = _mm_cvtss_si32(x1) & 0x7fffffff;
	if (limit > ExpVar<>::f88) {
		x1 = _mm_min_ss(x1, _mm_load_ss(expVar.maxX));
		x1 = _mm_max_ss(x1, _mm_load_ss(expVar.minX));
	}

	int r = _mm_cvtss_si32(_mm_mul_ss(x1, _mm_load_ss(expVar.a)));
	unsigned int v = r & mask(expVar.s);
	float t = _mm_cvtss_f32(x1) - r * expVar.b[0];
	int u = r >> expVar.s;
	fi fi;
	fi.i = ((u + 127) << 23) | expVar.tbl[v];
	return (1 + t) * fi.f;
}

inline double expd(double x)
{
	if (x <= -708.39641853226408) return 0;
	if (x >= 709.78271289338397) return std::numeric_limits<double>::infinity();
	using namespace local;
	const ExpdVar<>& c = C<>::expdVar;
	const double _b = double(uint64_t(3) << 51);
	__m128d b = _mm_load_sd(&_b);
	__m128d xx = _mm_load_sd(&x);
	__m128d d = _mm_add_sd(_mm_mul_sd(xx, _mm_load_sd(&c.a)), b);
	uint64_t di = _mm_cvtsi128_si32(_mm_castpd_si128(d));
	uint64_t iax = c.tbl[di & mask(c.sbit)];
	__m128d _t = _mm_sub_sd(_mm_mul_sd(_mm_sub_sd(d, b), _mm_load_sd(&c.ra)), xx);
	uint64_t u = ((di + c.adj) >> c.sbit) << 52;
	double t;
	_mm_store_sd(&t, _t);
	double y = (c.C3[0] - t) * (t * t) * c.C2[0] - t + c.C1[0];
	double did;
	u |= iax;
	memcpy(&did, &u, sizeof(did));
	return y * did;
}

/*
	px : pointer to array of double
	n : size of array(assume multiple of 2 or 4)
*/
inline void expd_v(double *px, size_t n)
{
	using namespace local;
	const ExpdVar<>& c = C<>::expdVar;
	const double b = double(3ULL << 51);
	size_t r = n & 1;
	n &= ~1;
	const __m128d mC1 = _mm_set1_pd(c.C1[0]);
	const __m128d mC2 = _mm_set1_pd(c.C2[0]);
	const __m128d mC3 = _mm_set1_pd(c.C3[0]);
	const __m128d ma = _mm_set1_pd(c.a);
	const __m128d mra = _mm_set1_pd(c.ra);
#if defined(__x86_64__) || defined(_WIN64)
	const __m128i madj = _mm_set1_epi64x(c.adj);
#else
	const __m128i madj = _mm_set_epi32(0, c.adj, 0, c.adj);
#endif
	const __m128d expMax = _mm_set1_pd(709.78272569338397);
	const __m128d expMin = _mm_set1_pd(-708.39641853226408);
	for (size_t i = 0; i < n; i += 2) {
		__m128d x = _mm_load_pd(px);
		x = _mm_min_pd(x, expMax);
		x = _mm_max_pd(x, expMin);

		__m128d d = _mm_mul_pd(x, ma);
		d = _mm_add_pd(d, _mm_set1_pd(b));
		int adr0 = _mm_cvtsi128_si32(_mm_castpd_si128(d)) & mask(c.sbit);
		int adr1 = _mm_cvtsi128_si32(_mm_srli_si128(_mm_castpd_si128(d), 8)) & mask(c.sbit);

		__m128i iaxL = _mm_castpd_si128(_mm_load_sd((const double*)&c.tbl[adr0]));
		__m128i iax = _mm_castpd_si128(_mm_load_sd((const double*)&c.tbl[adr1]));
		iax = _mm_unpacklo_epi64(iaxL, iax);

		__m128d t = _mm_sub_pd(_mm_mul_pd(_mm_sub_pd(d, _mm_set1_pd(b)), mra), x);
		__m128i u = _mm_castpd_si128(d);
		u = _mm_add_epi64(u, madj);
		u = _mm_srli_epi64(u, c.sbit);
		u = _mm_slli_epi64(u, 52);
		u = _mm_or_si128(u, iax);
		__m128d y = _mm_mul_pd(_mm_sub_pd(mC3, t), _mm_mul_pd(t, t));
		y = _mm_mul_pd(y, mC2);
		y = _mm_add_pd(_mm_sub_pd(y, t), mC1);
		_mm_store_pd(px, _mm_mul_pd(y, _mm_castsi128_pd(u)));
		px += 2;
	}
	for (size_t i = 0; i < r; i++) {
		px[i] = expd(px[i]);
	}
}

inline __m128 exp_ps(__m128 x)
{
	using namespace local;
	const ExpVar<>& expVar = C<>::expVar;

	__m128i limit = _mm_castps_si128(_mm_and_ps(x, *cast_to<__m128>(expVar.i7fffffff)));
	int over = _mm_movemask_epi8(_mm_cmpgt_epi32(limit, *cast_to<__m128i>(expVar.maxX)));
	if (over) {
		x = _mm_min_ps(x, _mm_load_ps(expVar.maxX));
		x = _mm_max_ps(x, _mm_load_ps(expVar.minX));
	}

	__m128i r = _mm_cvtps_epi32(_mm_mul_ps(x, *cast_to<__m128>(expVar.a)));
	__m128 t = _mm_sub_ps(x, _mm_mul_ps(_mm_cvtepi32_ps(r), *cast_to<__m128>(expVar.b)));
	t = _mm_add_ps(t, *cast_to<__m128>(expVar.f1));

	__m128i v4 = _mm_and_si128(r, *cast_to<__m128i>(expVar.mask_s));
	__m128i u4 = _mm_add_epi32(r, *cast_to<__m128i>(expVar.i127s));
	u4 = _mm_srli_epi32(u4, expVar.s);
	u4 = _mm_slli_epi32(u4, 23);

	unsigned int v0, v1, v2, v3;
	v0 = _mm_cvtsi128_si32(v4);
	v1 = _mm_extract_epi16(v4, 2);
	v2 = _mm_extract_epi16(v4, 4);
	v3 = _mm_extract_epi16(v4, 6);
	__m128 t0, t1, t2, t3;

	t0 = _mm_castsi128_ps(_mm_set1_epi32(expVar.tbl[v0]));
	t1 = _mm_castsi128_ps(_mm_set1_epi32(expVar.tbl[v1]));
	t2 = _mm_castsi128_ps(_mm_set1_epi32(expVar.tbl[v2]));
	t3 = _mm_castsi128_ps(_mm_set1_epi32(expVar.tbl[v3]));

	t1 = _mm_movelh_ps(t1, t3);
	t1 = _mm_castsi128_ps(_mm_slli_epi64(_mm_castps_si128(t1), 32));
	t0 = _mm_movelh_ps(t0, t2);
	t0 = _mm_castsi128_ps(_mm_srli_epi64(_mm_castps_si128(t0), 32));
	t0 = _mm_or_ps(t0, t1);
	t0 = _mm_or_ps(t0, _mm_castsi128_ps(u4));

	t = _mm_mul_ps(t, t0);

	return t;
}

inline float log(float x)
{
	using namespace local;
	const LogVar<>& logVar = C<>::logVar;
	const size_t logLen = logVar.LEN;

	fi fi;
	fi.f = x;
	int a = fi.i & (mask(8) << 23);
	unsigned int b1 = fi.i & (mask(logLen) << (23 - logLen));
	unsigned int b2 = fi.i & mask(23 - logLen);
	int idx = b1 >> (23 - logLen);
	float f = float(a - (127 << 23)) * logVar.c_log2 + logVar.tbl[idx].app + float(b2) * logVar.tbl[idx].rev;
	return f;
}

inline __m128 log_ps(__m128 x)
{
	using namespace local;
	const LogVar<>& logVar = C<>::logVar;

	__m128i xi = _mm_castps_si128(x);
	__m128i idx = _mm_srli_epi32(_mm_and_si128(xi, *cast_to<__m128i>(logVar.m2)), (23 - logVar.LEN));
	__m128 a  = _mm_cvtepi32_ps(_mm_sub_epi32(_mm_and_si128(xi, *cast_to<__m128i>(logVar.m1)), *cast_to<__m128i>(logVar.m5)));
	__m128 b2 = _mm_cvtepi32_ps(_mm_and_si128(xi, *cast_to<__m128i>(logVar.m3)));

	a = _mm_mul_ps(a, *cast_to<__m128>(logVar.m4)); // c_log2

	unsigned int i0 = _mm_cvtsi128_si32(idx);

	unsigned int i1 = _mm_extract_epi16(idx, 2);
	unsigned int i2 = _mm_extract_epi16(idx, 4);
	unsigned int i3 = _mm_extract_epi16(idx, 6);

	__m128 app, rev;
	__m128i L = _mm_loadl_epi64(cast_to<__m128i>(&logVar.tbl[i0].app));
	__m128i H = _mm_loadl_epi64(cast_to<__m128i>(&logVar.tbl[i1].app));
	__m128 t = _mm_castsi128_ps(_mm_unpacklo_epi64(L, H));
	L = _mm_loadl_epi64(cast_to<__m128i>(&logVar.tbl[i2].app));
	H = _mm_loadl_epi64(cast_to<__m128i>(&logVar.tbl[i3].app));
	rev = _mm_castsi128_ps(_mm_unpacklo_epi64(L, H));
	app = _mm_shuffle_ps(t, rev, MIE_PACK(2, 0, 2, 0));
	rev = _mm_shuffle_ps(t, rev, MIE_PACK(3, 1, 3, 1));

	a = _mm_add_ps(a, app);
	rev = _mm_mul_ps(b2, rev);
	return _mm_add_ps(a, rev);
}

} // fmath

#endif // FMATH_HPP_
