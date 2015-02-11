#ifndef RECURSIVEFILEITERATOR_HPP_
#define RECURSIVEFILEITERATOR_HPP_

#include "FileIterator.hpp"

#include <stack>

namespace Tungsten {

class RecursiveFileIterator
{
    std::stack<FileIterator> _iterators;

public:
    RecursiveFileIterator() = default;
    RecursiveFileIterator(const Path &p);

    bool operator==(const RecursiveFileIterator &o) const;
    bool operator!=(const RecursiveFileIterator &o) const;

    RecursiveFileIterator &operator++();
    RecursiveFileIterator  operator++(int);

    Path &operator*();
    const Path &operator*() const;
};

}



#endif /* RECURSIVEFILEITERATOR_HPP_ */
