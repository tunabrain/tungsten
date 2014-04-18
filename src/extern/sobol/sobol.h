// Copyright (c) 2012 Leonhard Gruenschloss (leonhard@gruenschloss.org)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is furnished to do
// so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef SOBOL_H
#define SOBOL_H

#include "IntTypes.hpp"

namespace sobol {

struct Matrices
{
    static const unsigned num_dimensions = 1024;
    static const unsigned size = 52;
    static const Tungsten::uint32 matrices[];
};

// Compute one component of the Sobol'-sequence, where the component
// corresponds to the dimension parameter, and the index specifies
// the point inside the sequence. The scramble parameter can be used
// to permute elementary intervals, and might be chosen randomly to
// generate a randomized QMC sequence.
inline Tungsten::uint32 sample(
    Tungsten::uint64 index,
    const unsigned dimension,
    const Tungsten::uint32 scramble = 0U)
{
    Tungsten::uint32 result = scramble;
    for (unsigned i = dimension * Matrices::size; index; index >>= 1, ++i)
    {
        if (index & 1)
            result ^= Matrices::matrices[i];
    }

    return result;
}

} // namespace sobol

#endif

