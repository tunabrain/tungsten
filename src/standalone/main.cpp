#include "Timer.hpp"

#include "renderer/TraceableScene.hpp"
#include "renderer/Renderer.hpp"

#include "cameras/Camera.hpp"

#include "io/FileUtils.hpp"
#include "io/Scene.hpp"

#include "extern/tinyformat/tinyformat.hpp"
#include "extern/embree/include/embree.h"
#include "extern/lodepng/lodepng.h"

using namespace Tungsten;

std::string incrementalFilename(const std::string &relFile,
                                const std::string &dstFile,
                                const std::string &suffix)
{
    std::string fileName = FileUtils::stripExt(dstFile) + suffix + "." + FileUtils::extractExt(dstFile);
    std::string dst = FileUtils::addSeparator(FileUtils::extractParent(relFile)) + fileName;
    std::string basename = FileUtils::stripExt(dst);
    std::string extension = FileUtils::extractExt(dst);
    int index = 0;
    while (FileUtils::fileExists(dst))
        dst = tfm::format("%s%05d.%s", basename, ++index, extension);

    return std::move(dst);
}

void saveFrameAndVariance(Renderer &renderer, Scene &scene, Camera &camera, const std::string &suffix)
{
    std::string dst         = incrementalFilename(scene.path(), camera.outputFile(), suffix);
    std::string dstVariance = incrementalFilename(scene.path(), camera.outputFile(), suffix + "Variance");

    int w = camera.resolution().x();
    int h = camera.resolution().y();
    std::unique_ptr<uint8[]> ldr(new uint8[w*h*3]);
    std::unique_ptr<Vec3f[]> hdr(new Vec3f[w*h]);
    for (int y = 0, idx = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x, ++idx) {
            Vec3c rgb(clamp(camera.get(x, y)*256.0f, Vec3f(0.0f), Vec3f(255.0f)));
            ldr[idx*3 + 0] = rgb.x();
            ldr[idx*3 + 1] = rgb.y();
            ldr[idx*3 + 2] = rgb.z();

            hdr[idx] = camera.getLinear(x, h - y - 1);
        }
    }

    FileUtils::createDirectory(FileUtils::extractParent(dst));

    lodepng_encode24_file(dst.c_str(), ldr.get(), w, h);
    renderer.saveVariance(dstVariance);

    std::string pfmDst = FileUtils::stripExt(dst) + ".pfm";
    std::ofstream pfmOut(pfmDst, std::ios_base::out | std::ios_base::binary);
    if (!pfmOut.good()) {
        std::cout << "Warning: Unable to open pfm output at" << pfmOut << std::endl;
        return;
    }
    pfmOut << "PF\n" << w << " " << h << "\n" << -1.0 << "\n";
    pfmOut.write(reinterpret_cast<const char *>(hdr.get()), w*h*sizeof(Vec3f));
    pfmOut.close();
}

std::string formatTime(double elapsed)
{
    uint64 seconds = uint64(elapsed);
    uint64 minutes = seconds/60;
    uint64 hours = minutes/60;
    uint64 days = hours/24;
    double fraction = elapsed - seconds;

    std::stringstream ss;

    if (days)    ss << tfm::format("%dd ", days);
    if (hours)   ss << tfm::format("%dh ", hours % 24);
    if (minutes) ss << tfm::format("%dm ", minutes % 60);
    if (seconds) ss << tfm::format("%ds %dms", seconds % 60, uint64(fraction*1000.0f) % 1000);
    else ss << elapsed << "s";

    return ss.str();
}

int main(int argc, const char *argv[])
{
    constexpr int ThreadCount = 7;
    constexpr int SppStep = 16;
    constexpr int BackupInterval = 60*15;

    if (argc < 2) {
        std::cerr << "Usage: tungsten scene1 [scene2 [scene3....]]\n";
        return 1;
    }

    embree::rtcInit();
    //embree::rtcSetVerbose(1);
    embree::rtcStartThreads(ThreadCount);

    for (int i = 1; i < argc; ++i) {
        std::cout << tfm::format("Loading scene '%s'...", argv[i]) << std::endl;
        std::unique_ptr<Scene> scene;
        try {
            scene.reset(Scene::load(argv[i]));
        } catch (std::runtime_error &e) {
            std::cerr << tfm::format("Scene loader for file '%s' encountered an unrecoverable error: \n%s",
                    argv[i], e.what()) << std::endl;
            continue;
        }

        try {
            int maxSpp = scene->camera()->spp();
            //int maxSpp = 2048;
            std::unique_ptr<TraceableScene> flattenedScene(scene->makeTraceable());
            std::unique_ptr<Renderer> renderer(new Renderer(*flattenedScene, ThreadCount));

            std::cout << "Starting render..." << std::endl;
            Timer timer, checkpointTimer;
            double totalElapsed = 0.0;
            for (int i = 0; i < maxSpp; i += SppStep) {
                renderer->startRender([](){}, i, min(i + SppStep, maxSpp));
                renderer->waitForCompletion();
                std::cout << tfm::format("Completed %d/%d spp", min(i + SppStep, maxSpp), maxSpp) << std::endl;
                checkpointTimer.stop();
                if (checkpointTimer.elapsed() > BackupInterval) {
                    totalElapsed += checkpointTimer.elapsed();
                    std::cout << tfm::format("Saving checkpoint after %s", formatTime(totalElapsed).c_str()) << std::endl;
                    checkpointTimer.start();
                    saveFrameAndVariance(*renderer, *scene, *scene->camera(), "Checkpoint");
                }
            }
            timer.stop();

            std::cout << tfm::format("Finished render. Render time %s", formatTime(timer.elapsed()).c_str()) << std::endl;

            saveFrameAndVariance(*renderer, *scene, *scene->camera(), "");
        } catch (std::runtime_error &e) {
            std::cerr << tfm::format("Renderer for file '%s' encountered an unrecoverable error: \n%s",
                    argv[i], e.what()) << std::endl;
        }
    }

    return 0;
}

