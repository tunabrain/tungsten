#ifndef TRANSMITTANCEFACTORY_HPP_
#define TRANSMITTANCEFACTORY_HPP_

#include "StringableEnum.hpp"

#include <functional>
#include <memory>

namespace Tungsten {

class Transmittance;

typedef StringableEnum<std::function<std::shared_ptr<Transmittance>()>> TransmittanceFactory;

}

#endif /* TRANSMITTANCEFACTORY_HPP_ */
