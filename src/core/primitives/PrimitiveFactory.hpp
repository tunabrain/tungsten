#ifndef PRIMITIVEFACTORY_HPP_
#define PRIMITIVEFACTORY_HPP_

#include "StringableEnum.hpp"

#include <functional>
#include <memory>

namespace Tungsten {

class Primitive;

typedef StringableEnum<std::function<std::shared_ptr<Primitive>()>> PrimitiveFactory;

}

#endif /* PRIMITIVEFACTORY_HPP_ */
