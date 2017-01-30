#ifndef INTEGRATORFACTORY_HPP_
#define INTEGRATORFACTORY_HPP_

#include "StringableEnum.hpp"

#include <functional>
#include <memory>

namespace Tungsten {

class Integrator;

typedef StringableEnum<std::function<std::shared_ptr<Integrator>()>> IntegratorFactory;

}

#endif /* INTEGRATORFACTORY_HPP_ */
