#ifndef WRITABLEPATHSAMPLEGENERATOR_HPP_
#define WRITABLEPATHSAMPLEGENERATOR_HPP_

#include "PathSampleGenerator.hpp"

namespace Tungsten {

class WritablePathSampleGenerator : public PathSampleGenerator
{
public:
    virtual ~WritablePathSampleGenerator() {}

    virtual void seek(int vertex) = 0;
    virtual void putBoolean(float pTrue, bool choice) = 0;
    virtual void putDiscrete(int numChoices, int choice) = 0;
    virtual void put1D(float value) = 0;
    virtual void put2D(Vec2f value) = 0;

    virtual float untracked1D() = 0;
    inline Vec2f untracked2D()
    {
        float a = untracked1D();
        float b = untracked1D();
        return Vec2f(a, b);
    }
    inline bool untrackedBoolean(float pTrue)
    {
        return untracked1D() < pTrue;
    }
    inline int untrackedDiscrete(int numChoices)
    {
        return int(untracked1D()*numChoices);
    }
};

}

#endif /* WRITABLEPATHSAMPLEGENERATOR_HPP_ */
