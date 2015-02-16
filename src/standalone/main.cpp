#include "Version.hpp"
#include "Timer.hpp"

#include "renderer/TraceableScene.hpp"
#include "renderer/Renderer.hpp"

#include "cameras/Camera.hpp"

#include "thread/ThreadUtils.hpp"

#include "io/DirectoryChange.hpp"
#include "io/FileUtils.hpp"
#include "io/CliParser.hpp"
#include "io/Scene.hpp"

#include <tinyformat/tinyformat.hpp>
#include <embree/include/embree.h>

namespace Tungsten {

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

}

using namespace Tungsten;

static const int OPT_CHECKPOINTS       = 0;
static const int OPT_THREADS           = 1;
static const int OPT_VERSION           = 2;
static const int OPT_HELP              = 3;

int main(int argc, const char *argv[])
{
    int checkpointInterval = 0;
    int threadCount = max(ThreadUtils::idealThreadCount() - 1, 1u);

    CliParser parser("tungsten", "[options] scene1 [scene2 [scene3...]]");
    parser.addOption('h', "help", "Prints this help text", false, OPT_HELP);
    parser.addOption('v', "version", "Prints version information", false, OPT_VERSION);
    parser.addOption('t', "threads", "Specifies number of threads to use (default: number of cores minus one)", true, OPT_THREADS);
    parser.addOption('c', "checkpoint", "Specifies render time in minutes before saving a checkpoint. A value of 0 disables checkpoints. Overrides the setting in the scene file", true, OPT_CHECKPOINTS);

    parser.parse(argc, argv);

    if (argc < 2 || parser.isPresent(OPT_HELP)) {
        parser.printHelpText();
        return 0;
    }
    if (parser.isPresent(OPT_VERSION)) {
        std::cout << "tungsten, version " << VERSION_STRING << std::endl;
        return 0;
    }

    if (parser.isPresent(OPT_THREADS)) {
        int newThreadCount = std::atoi(parser.param(OPT_THREADS).c_str());
        if (newThreadCount > 0)
            threadCount = newThreadCount;
    }
    if (parser.isPresent(OPT_CHECKPOINTS))
        checkpointInterval = std::atoi(parser.param(OPT_CHECKPOINTS).c_str());

    embree::rtcInit();
    //embree::rtcSetVerbose(1);
    embree::rtcStartThreads(threadCount);

    ThreadUtils::startThreads(threadCount);

    for (const std::string &sceneFile : parser.operands()) {
        std::cout << tfm::format("Loading scene '%s'...", sceneFile) << std::endl;
        std::unique_ptr<Scene> scene;
        try {
            scene.reset(Scene::load(Path(sceneFile)));
            scene->loadResources();
        } catch (std::runtime_error &e) {
            std::cerr << tfm::format("Scene loader for file '%s' encountered an unrecoverable error: \n%s",
                    sceneFile, e.what()) << std::endl;
            continue;
        }

        try {
            DirectoryChange context(scene->path().parent());

            int maxSpp = scene->rendererSettings().spp();
            std::unique_ptr<TraceableScene> flattenedScene(scene->makeTraceable());
            std::unique_ptr<Renderer> renderer(new Renderer(*flattenedScene));

            if (!parser.isPresent(OPT_CHECKPOINTS))
                checkpointInterval = scene->rendererSettings().checkpointInterval();

            std::cout << "Starting render..." << std::endl;
            Timer timer, checkpointTimer;
            double totalElapsed = 0.0;
            while (!renderer->done()) {
                renderer->startRender([](){});
                renderer->waitForCompletion();
                std::cout << tfm::format("Completed %d/%d spp", renderer->currentSpp(), maxSpp) << std::endl;
                checkpointTimer.stop();
                if (checkpointInterval > 0 && checkpointTimer.elapsed() > checkpointInterval*60) {
                    totalElapsed += checkpointTimer.elapsed();
                    std::cout << tfm::format("Saving checkpoint after %s", formatTime(totalElapsed).c_str()) << std::endl;
                    checkpointTimer.start();
                    renderer->saveCheckpoint();
                }
            }
            timer.stop();

            std::cout << tfm::format("Finished render. Render time %s", formatTime(timer.elapsed()).c_str()) << std::endl;

            renderer->saveOutputs();
        } catch (std::runtime_error &e) {
            std::cerr << tfm::format("Renderer for file '%s' encountered an unrecoverable error: \n%s",
                    sceneFile, e.what()) << std::endl;
        }
    }

    return 0;
}

