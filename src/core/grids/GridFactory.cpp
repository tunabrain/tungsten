#include "GridFactory.hpp"

#include "VdbGrid.hpp"

namespace Tungsten {

#if OPENVDB_AVAILABLE
#define OPENVDB_ENTRY {"vdb", std::make_shared<VdbGrid>},
#else
#define OPENVDB_ENTRY
#endif

DEFINE_STRINGABLE_ENUM(GridFactory, "grid", ({
    OPENVDB_ENTRY
}))

}
