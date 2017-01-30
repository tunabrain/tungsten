#ifndef BSDFFACTORY_HPP_
#define BSDFFACTORY_HPP_

#include "StringableEnum.hpp"

#include <functional>
#include <memory>

namespace Tungsten {

class Bsdf;

typedef StringableEnum<std::function<std::shared_ptr<Bsdf>()>> BsdfFactory;

}

#endif /* BSDFFACTORY_HPP_ */
