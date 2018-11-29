#include "TransmittanceFactory.hpp"
#include "DoubleExponentialTransmittance.hpp"
#include "DavisWeinsteinTransmittance.hpp"
#include "InterpolatedTransmittance.hpp"
#include "ExponentialTransmittance.hpp"
#include "QuadraticTransmittance.hpp"
#include "ErlangTransmittance.hpp"
#include "LinearTransmittance.hpp"
#include "DavisTransmittance.hpp"
#include "PulseTransmittance.hpp"

namespace Tungsten {

DEFINE_STRINGABLE_ENUM(TransmittanceFactory, "transmittance", ({
    {"double_exponential", std::make_shared<DoubleExponentialTransmittance>},
    {"exponential", std::make_shared<ExponentialTransmittance>},
    {"quadratic", std::make_shared<QuadraticTransmittance>},
    {"linear", std::make_shared<LinearTransmittance>},
    {"erlang", std::make_shared<ErlangTransmittance>},
    {"davis", std::make_shared<DavisTransmittance>},
    {"davis_weinstein", std::make_shared<DavisWeinsteinTransmittance>},
    {"pulse", std::make_shared<PulseTransmittance>},
    {"interpolated", std::make_shared<InterpolatedTransmittance>},
}))

}
