#ifndef ALIGNEDALLOCATOR_HPP_
#define ALIGNEDALLOCATOR_HPP_

#ifdef __APPLE__
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif
#include <memory>

template <class T, size_t Alignment>
struct AlignedAllocator : public std::allocator<T>
{
        typedef typename std::allocator<T>::size_type size_type;
        typedef typename std::allocator<T>::pointer pointer;
        typedef typename std::allocator<T>::const_pointer const_pointer;

        template <class U>
        struct rebind
        {
            typedef AlignedAllocator<U, Alignment> other;
        };

        AlignedAllocator() throw() { }

        AlignedAllocator(const AlignedAllocator &other) throw()
        : std::allocator<T>(other)
        {
        }

        template <class U>
        AlignedAllocator(const AlignedAllocator<U, Alignment> &) throw()
        {
        }

        ~AlignedAllocator() throw()
        {
        }

        template <class T1, size_t A1>
        bool operator==(const AlignedAllocator<T1,A1> &) const
        {
            return true;
        }

        template <class T1, size_t A1>
        bool operator!=(const AlignedAllocator<T1,A1> &) const
        {
            return false;
        }

        pointer allocate(size_type n)
        {
            return allocate(n, const_pointer(0));
        }

        pointer allocate(size_type n, const_pointer /*hint*/)
        {
            void *ptr;
#ifdef _WIN32
            ptr = _aligned_malloc(n*sizeof(T), Alignment);
#else
            if (posix_memalign(&ptr, Alignment, n*sizeof(T)))
                ptr = NULL;
#endif
            if (!ptr)
                throw std::bad_alloc();
            return static_cast<pointer>(ptr);
        }

        void deallocate(pointer p, size_type /*n*/)
        {
#ifdef _WIN32
            _aligned_free(p);
#else
            free(p);
#endif
        }
};

#endif /* ALIGNEDALLOCATOR_HPP_ */
