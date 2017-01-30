#ifndef MEDIUMFACTORY_HPP_
#define MEDIUMFACTORY_HPP_

#include "StringableEnum.hpp"

#include <functional>
#include <memory>

namespace Tungsten {

class Medium;

typedef StringableEnum<std::function<std::shared_ptr<Medium>()>> MediumFactory;

}

#endif /* MEDIUMFACTORY_HPP_ */
