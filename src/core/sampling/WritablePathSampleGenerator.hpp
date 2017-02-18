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
};

}

#endif /* WRITABLEPATHSAMPLEGENERATOR_HPP_ */
