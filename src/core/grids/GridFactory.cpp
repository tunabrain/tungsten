#include "GridFactory.hpp"

#include "VdbGrid.hpp"

namespace Tungsten {

DEFINE_STRINGABLE_ENUM(GridFactory, "grid", ({
#if OPENVDB_AVAILABLE
    {"vdb", std::make_shared<VdbGrid>},
#endif
}))

}
