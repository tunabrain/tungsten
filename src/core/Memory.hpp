#ifndef MEMORY_HPP_
#define MEMORY_HPP_

#include <cstring>
#include <memory>

namespace Tungsten {

template<typename T>
inline std::unique_ptr<T[]> zeroAlloc(size_t size)
{
    std::unique_ptr<T[]> result(new T[size]);
    std::memset(result.get(), 0, size*sizeof(T));
    return std::move(result);
}

}

#endif /* MEMORY_HPP_ */
