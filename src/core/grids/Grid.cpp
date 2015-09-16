#include "Grid.hpp"

namespace Tungsten {

Mat4f Grid::naturalTransform() const
{
    return Mat4f();
}

Mat4f Grid::invNaturalTransform() const
{
    return Mat4f();
}

Box3f Grid::bounds() const
{
    return Box3f();
}

}
