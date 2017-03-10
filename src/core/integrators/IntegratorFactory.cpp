#include "IntegratorFactory.hpp"

#include "bidirectional_path_tracer/BidirectionalPathTraceIntegrator.hpp"
#include "progressive_photon_map/ProgressivePhotonMapIntegrator.hpp"
#include "reversible_jump_mlt/ReversibleJumpMltIntegrator.hpp"
#include "multiplexed_mlt/MultiplexedMltIntegrator.hpp"
#include "light_tracer/LightTraceIntegrator.hpp"
#include "kelemen_mlt/KelemenMltIntegrator.hpp"
#include "path_tracer/PathTraceIntegrator.hpp"
#include "photon_map/PhotonMapIntegrator.hpp"

namespace Tungsten {

DEFINE_STRINGABLE_ENUM(IntegratorFactory, "integrator", ({
    {"path_tracer", std::make_shared<PathTraceIntegrator>},
    {"light_tracer", std::make_shared<LightTraceIntegrator>},
    {"photon_map", std::make_shared<PhotonMapIntegrator>},
    {"progressive_photon_map", std::make_shared<ProgressivePhotonMapIntegrator>},
    {"bidirectional_path_tracer", std::make_shared<BidirectionalPathTraceIntegrator>},
    {"kelemen_mlt", std::make_shared<KelemenMltIntegrator>},
    {"multiplexed_mlt", std::make_shared<MultiplexedMltIntegrator>},
    {"reversible_jump_mlt", std::make_shared<ReversibleJumpMltIntegrator>},
}))

}
