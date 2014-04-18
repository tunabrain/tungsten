#include <common/ray.h>
#include <iostream>
#include <thread>
#include <chrono>

#include <tbb/concurrent_queue.h>

#include "integrators/PathTraceIntegrator.hpp"

#include "primitives/HeightmapTerrain.hpp"

#include "materials/AtmosphericScattering.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/LightSample.hpp"

#include "extern/lodepng/lodepng.h"

#include "lights/ConeLight.hpp"

#include "bsdfs/LambertBsdf.hpp"

#include "math/TangentSpace.hpp"
#include "math/MathUtil.hpp"
#include "math/Angle.hpp"

#include "io/HfaLoader.hpp"
#include "io/ObjLoader.hpp"
#include "io/Scene.hpp"

#include "Renderer.hpp"
#include "Camera.hpp"


namespace Tungsten
{

Vec3f tonemap(const Vec3f &c)
{
    //return min(c, Vec3f(1.0f));
    Vec3f x(max(c - 0.004f, Vec3f(0.0f)));
    x = (x*(6.2f*x + 0.5f))/(x*(6.2f*x + 1.7f) + 0.06f);
    return min(x, Vec3f(1.0f));
    /*return c;
    constexpr float gamma = 1.0f/2.2f;
    Vec3f monitor(
        std::pow(c.x(), gamma),
        std::pow(c.y(), gamma),
        std::pow(c.z(), gamma)
    );

    return min(monitor, Vec3f(1.0f));*/
}

}

using namespace Tungsten;

int main()
{
    constexpr uint32 spp = 4;
    constexpr uint32 ThreadCount = 6;
    Vec2u resolution(1920u, 1080u);

    embree::rtcInit();
    embree::rtcSetVerbose(1);
    embree::rtcStartThreads(ThreadCount);

    //PackedGeometry *mesh = ObjLoader::load("models/CornellBox/CornellBox-Glossy.obj");
    //Mesh *mesh = ObjLoader::load("models/DrabovicSponza/sponza.obj");

    //Scene scene(*ObjLoader::load("models/DrabovicSponza/sponza.obj"));
    //Scene scene(*ObjLoader::load("models/SanMiguel/san-miguel.obj"));
    //Scene scene(*ObjLoader::load("models/LostEmpire/lost_empire.obj"));
    //Scene scene = ObjLoader::load("models/CornellBox/CornellBox-Glossy.obj");
    //Scene scene = Scene::load("TestConversion/Test.scene");
    //Scene scene = Scene::load("SanMiguel/scene.json");
    //Scene scene = Scene::load("LostEmpire/scene.json");
    //Scene scene = Scene::load("Sponza/scene.json");

    Scene scene;

    //Scene::save("Sponza/scene.json", scene);
    //return 0;

    std::vector<std::shared_ptr<Light>> lights = std::move(scene.lights());
    //delete scene;

    //Vec3f sun = Vec3f(0.5f, -1.0f, -0.3f).normalized();
    //Vec3f sun = Vec3f(0.3f, -1.0f, -0.1f).normalized();
    Vec3f sun = Vec3f(-0.3f, -0.2f, 1.0f).normalized();

    lights.emplace_back(new ConeLight(
        9.35e-3f*9.0f,
        sun,
        Vec3f(600.0f))
    );

    //Mat4f transform = Mat4f::translate(Vec3f(23.57511f, 9.77478f, 2.23092f + 0.1f))*Mat4f::rotXYZ(Vec3f(52.941f - 90.0f, -249.706f, 0.0f));
    //Mat4f transform = Mat4f::translate(Vec3f(-13.0f, 2.0f, 0.0f))*Mat4f::rotYZX(Vec3f(-15.0f, -90.0f, 0.0f));
    Mat4f transform = Mat4f::translate(Vec3f(0.0f, 100.0f, 0.0f))*Mat4f::rotYZX(Vec3f(10.0f, 0.0f, 0.0f));

    //Camera cam(Vec3f(0.0f, 20.0f, 130.0f), resolution, degToRad(60.0f));
    Camera cam(transform, resolution, Angle::degToRad(70.0f));


    HfaLoader loader("C:/Users/Tuna brain/Desktop/Terrain/imgn37w116_13.img", "");
    MinMaxChain heightmap(loader.mapData(), loader.w(), loader.h());
    uint32 centerX = loader.w()/2 - 500;
    uint32 centerY = loader.h()/2 - 2600;
    Vec3f center(
        centerX,
        heightmap.at(centerX, centerY),
        centerY
    );
    std::cout << center.y() << std::endl;
    Mat4f terrainTform = Mat4f::scale(Vec3f(loader.xScale(), 1.0f, loader.yScale()))*Mat4f::translate(-center);
    HeightmapTerrain terrain(cam, heightmap, terrainTform, 1.0f);
    scene.addMesh(terrain.buildMesh(
        std::shared_ptr<Material>(new Material(std::shared_ptr<Bsdf>(new LambertBsdf(Vec3f(0.5f))), Vec3f(0.0f), "default")),
        "Terrain"
    ));

    PackedGeometry mesh(scene.flatten());

    Vec3c *img = new Vec3c[resolution.x()*resolution.y()];

    AtmosphericScattering atmosphere(AtmosphereParameters::generic());
    atmosphere.precompute();

    std::cout << "Starting render..." << std::endl;

    std::chrono::time_point<std::chrono::high_resolution_clock> start = std::chrono::high_resolution_clock::now();

    Renderer<PathTraceIntegrator> renderer(cam, mesh, lights);

    auto setPixel = [&](uint32 x, uint32 y, const Vec3f &c) {
        img[x + y*resolution.x()] = Vec3c(tonemap(c)*255.0f);
    };

    renderer.startRender(setPixel, spp, ThreadCount);
    renderer.waitForCompletion();

    std::chrono::time_point<std::chrono::high_resolution_clock> end = std::chrono::high_resolution_clock::now();

    double elapsed = std::chrono::duration<double>(end - start).count();

    uint64 samples = resolution.x()*resolution.y()*spp;
    std::cout << "Render time: " << elapsed << " seconds at ";
    std::cout.precision(10);
    std::cout << samples/elapsed;
    std::cout << " samples/s or " << samples/(elapsed*ThreadCount) << " samples/s per core" << std::endl;

    lodepng_encode24_file("Tmp.png", img[0].data(), resolution.x(), resolution.y());

    return 0;
}

