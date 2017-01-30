#ifndef PHASEFUNCTIONFACTORY_HPP_
#define PHASEFUNCTIONFACTORY_HPP_

#include "StringableEnum.hpp"

#include <functional>
#include <memory>

namespace Tungsten {

class PhaseFunction;

typedef StringableEnum<std::function<std::shared_ptr<PhaseFunction>()>> PhaseFunctionFactory;

}

#endif /* PHASEFUNCTIONFACTORY_HPP_ */
