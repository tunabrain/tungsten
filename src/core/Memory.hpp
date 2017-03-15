#ifndef MEMORY_HPP_
#define MEMORY_HPP_

#include <cstring>
#include <memory>

#ifdef __APPLE__
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

namespace Tungsten {

template<typename T>
inline std::unique_ptr<T[]> zeroAlloc(size_t size)
{
    std::unique_ptr<T[]> result(new T[size]);
    std::memset(result.get(), 0, size*sizeof(T));
    return std::move(result);
}

template<class T>
struct AlignedDeleter
{
    void operator()(T *p) const
    {
#ifdef _WIN32
            _aligned_free(p);
#else
            free(p);
#endif
    }
};

template<class T>
using aligned_unique_ptr = std::unique_ptr<T[], AlignedDeleter<T>>;

template<typename T>
inline aligned_unique_ptr<T> alignedAlloc(size_t size, int alignment)
{
    void *ptr;
#ifdef _WIN32
    ptr = _aligned_malloc(size*sizeof(T), alignment);
#else
    if (posix_memalign(&ptr, alignment, size*sizeof(T)))
        ptr = NULL;
#endif
    if (!ptr)
        return nullptr;
    return aligned_unique_ptr<T>(static_cast<T *>(ptr));
}

template<typename T>
inline aligned_unique_ptr<T> alignedZeroAlloc(size_t size, int alignment)
{
    auto result = alignedAlloc<T>(size, alignment);
    std::memset(result.get(), 0, size*sizeof(T));
    return std::move(result);
}

}

#endif /* MEMORY_HPP_ */
