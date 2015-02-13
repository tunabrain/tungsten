#ifndef PLATFORM_HPP_
#define PLATFORM_HPP_

#if defined(_MSC_VER)
#define FORCE_INLINE __forceinline
#elif defined(__GNUC__)
#define FORCE_INLINE inline __attribute__((always_inline))
#else
#define FORCE_INLINE
#endif

#ifdef _MSC_VER 
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

#define MARK_UNUSED(x) (void)(x)

#endif /* PLATFORM_HPP_ */
