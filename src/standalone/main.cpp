#include "Timer.hpp"

#include "renderer/TraceableScene.hpp"
#include "renderer/Renderer.hpp"

#include "cameras/Camera.hpp"

#include "io/DirectoryChange.hpp"
#include "io/FileUtils.hpp"
#include "io/Scene.hpp"

#include <tinyformat/tinyformat.hpp>
#include <embree/include/embree.h>
#include <lodepng/lodepng.h>

using namespace Tungsten;

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
            DirectoryChange context(FileUtils::extractParent(scene->path()));

            int maxSpp = scene->camera()->spp();
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
                    scene->camera()->saveCheckpoint(*renderer);
                }
            }
            timer.stop();

            std::cout << tfm::format("Finished render. Render time %s", formatTime(timer.elapsed()).c_str()) << std::endl;

            scene->camera()->saveOutputs(*renderer);
        } catch (std::runtime_error &e) {
            std::cerr << tfm::format("Renderer for file '%s' encountered an unrecoverable error: \n%s",
                    argv[i], e.what()) << std::endl;
        }
    }

    return 0;
}

