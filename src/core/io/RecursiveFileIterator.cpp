#include "RecursiveFileIterator.hpp"

namespace Tungsten {

RecursiveFileIterator::RecursiveFileIterator(const Path &p)
{
    _iterators.emplace(p, false, false, Path());
}

bool RecursiveFileIterator::operator==(const RecursiveFileIterator &o) const
{
    bool  thisEmpty =   _iterators.empty() ||   _iterators.top() == FileIterator();
    bool otherEmpty = o._iterators.empty() || o._iterators.top() == FileIterator();

    return thisEmpty == otherEmpty;
}

bool RecursiveFileIterator::operator!=(const RecursiveFileIterator &o) const
{
    return !((*this) == o);
}

RecursiveFileIterator &RecursiveFileIterator::operator++()
{
    do {
        if (_iterators.top() == FileIterator()) {
            _iterators.pop();
            if (!_iterators.empty())
                _iterators.top()++;
        } else {
            if ((*_iterators.top()).isDirectory())
                _iterators.emplace((*_iterators.top()).begin());
            else
                _iterators.top()++;
        }
    } while (!_iterators.empty() && _iterators.top() == FileIterator());

    return *this;
}

RecursiveFileIterator RecursiveFileIterator::operator++(int)
{
    RecursiveFileIterator copy(*this);
    ++(*this);
    return copy;
}

Path &RecursiveFileIterator::operator*()
{
    return *_iterators.top();
}

const Path &RecursiveFileIterator::operator*() const
{
    return *_iterators.top();
}

}
