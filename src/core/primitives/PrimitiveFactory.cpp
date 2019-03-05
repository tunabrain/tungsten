#include "PrimitiveFactory.hpp"

#include "mc-loader/TraceableMinecraftMap.hpp"
#include "InfiniteSphereCap.hpp"
#include "InfiniteSphere.hpp"
#include "TriangleMesh.hpp"
#include "Cylinder.hpp"
#include "Instance.hpp"
#include "Skydome.hpp"
#include "Sphere.hpp"
#include "Curves.hpp"
#include "Point.hpp"
#include "Cube.hpp"
#include "Quad.hpp"
#include "Disk.hpp"

namespace Tungsten {

DEFINE_STRINGABLE_ENUM(PrimitiveFactory, "primitive", ({
    {"mesh", std::make_shared<TriangleMesh>},
    {"cube", std::make_shared<Cube>},
    {"sphere", std::make_shared<Sphere>},
    {"quad", std::make_shared<Quad>},
    {"disk", std::make_shared<Disk>},
    {"curves", std::make_shared<Curves>},
    {"point", std::make_shared<Point>},
    {"skydome", std::make_shared<Skydome>},
    {"cylinder", std::make_shared<Cylinder>},
    {"instances", std::make_shared<Instance>},
    {"infinite_sphere", std::make_shared<InfiniteSphere>},
    {"infinite_sphere_cap", std::make_shared<InfiniteSphereCap>},
    {"minecraft_map", std::make_shared<MinecraftLoader::TraceableMinecraftMap>},
}))

}
