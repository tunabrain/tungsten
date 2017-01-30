#ifndef GRIDFACTORY_HPP_
#define GRIDFACTORY_HPP_

#include "StringableEnum.hpp"

#include <functional>
#include <memory>

namespace Tungsten {

class Grid;

typedef StringableEnum<std::function<std::shared_ptr<Grid>()>> GridFactory;

}

#endif /* GRIDFACTORY_HPP_ */
