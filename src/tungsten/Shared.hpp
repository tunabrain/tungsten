#ifndef SHARED_HPP_
#define SHARED_HPP_

#include "renderer/TraceableScene.hpp"

#include "thread/ThreadUtils.hpp"

#include "io/DirectoryChange.hpp"
#include "io/StringUtils.hpp"
#include "io/FileUtils.hpp"
#include "io/JsonUtils.hpp"
#include "io/CliParser.hpp"
#include "io/Scene.hpp"

#include "Timer.hpp"

#include <tinyformat/tinyformat.hpp>
#include <embree/include/embree.h>
#include <rapidjson/document.h>
#include <cstdlib>
#include <vector>
#include <mutex>
#include <deque>

namespace Tungsten {

static const int OPT_CHECKPOINTS       = 0;
static const int OPT_THREADS           = 1;
static const int OPT_VERSION           = 2;
static const int OPT_HELP              = 3;
static const int OPT_RESTART           = 4;
static const int OPT_OUTPUT_DIRECTORY  = 5;
static const int OPT_SPP               = 6;
static const int OPT_SEED              = 7;
static const int OPT_TIMEOUT           = 8;

enum RenderState
{
    STATE_LOADING,
    STATE_RENDERING,
};

static const char *renderStateToString(RenderState state)
{
    switch (state) {
    case STATE_LOADING:   return "loading";
    case STATE_RENDERING: return "rendering";
    default:              return "unknown";
    }
}

struct RendererStatus
{
    RenderState state;
    int startSpp;
    int currentSpp;
    int nextSpp;
    int totalSpp;

    std::vector<Path> completedScenes;
    Path currentScene;
    std::deque<Path> queuedScenes;

    rapidjson::Value toJson(rapidjson::Document::AllocatorType &allocator) const
    {
        rapidjson::Value result(rapidjson::kObjectType);
        result.SetObject();

        result.AddMember("state", renderStateToString(state), allocator);
        result.AddMember("start_spp", startSpp, allocator);
        result.AddMember("current_spp", currentSpp, allocator);
        result.AddMember("next_spp", nextSpp, allocator);
        result.AddMember("total_spp", totalSpp, allocator);
        result.AddMember("current_scene", currentScene.asString().c_str(), allocator);
        rapidjson::Value completedValue(rapidjson::kArrayType);
        for (const Path &p : completedScenes)
            completedValue.PushBack(p.asString().c_str(), allocator);
        result.AddMember("completed_scenes", std::move(completedValue), allocator);
        rapidjson::Value queuedValue(rapidjson::kArrayType);
        for (const Path &p : queuedScenes)
            queuedValue.PushBack(p.asString().c_str(), allocator);
        result.AddMember("queued_scenes", std::move(queuedValue), allocator);

        return std::move(result);
    }
};

class StandaloneRenderer
{
    CliParser &_parser;
    std::ostream &_logStream;

    double _checkpointInterval;
    double _timeout;
    int _threadCount;
    Path _outputDirectory;

    std::unique_ptr<Scene> _scene;
    std::unique_ptr<TraceableScene> _flattenedScene;

    std::mutex _statusMutex;
    std::mutex _logMutex;
    std::mutex _sceneMutex;
    RendererStatus _status;

    void writeLogLine(const std::string &s)
    {
        std::unique_lock<std::mutex> lock(_logMutex);
        _logStream << s << std::endl;
    }

public:
    StandaloneRenderer(CliParser &parser, std::ostream &logStream)
    : _parser(parser),
      _logStream(logStream),
      _checkpointInterval(0.0),
      _timeout(0.0),
      _threadCount(max(ThreadUtils::idealThreadCount() - 1, 1u))
    {
        _status.state = STATE_LOADING;
        _status.currentSpp = _status.nextSpp = _status.totalSpp = 0;

        parser.addOption('h', "help", "Prints this help text", false, OPT_HELP);
        parser.addOption('v', "version", "Prints version information", false, OPT_VERSION);
        parser.addOption('t', "threads", "Specifies number of threads to use (default: number of cores minus one)", true, OPT_THREADS);
        parser.addOption('r', "restart", "Ignores saved render checkpoints and starts fresh from 0 spp", false, OPT_RESTART);
        parser.addOption('c', "checkpoint", "Specifies render time before saving a checkpoint. A value of 0 (default) disables checkpoints. Overrides the setting in the scene file", true, OPT_CHECKPOINTS);
        parser.addOption('o', "output-directory", "Specifies the output directory. Overrides the setting in the scene file", true, OPT_OUTPUT_DIRECTORY);
        parser.addOption('\0', "spp", "Sets the number of samples per pixel to render at. Overrides the setting in the scene file", true, OPT_SPP);
        parser.addOption('\0', "timeout", "Specifies the maximum render time. A value of 0 (default) means unlimited. Overrides the setting in the scene file", true, OPT_TIMEOUT);
        parser.addOption('s', "seed", "Specifies the random seed to use", true, OPT_SEED);
    }

    void setup()
    {
        if (_parser.operands().empty() || _parser.isPresent(OPT_HELP)) {
            _parser.printHelpText();
            std::exit(0);
        }

        if (_parser.isPresent(OPT_THREADS)) {
            int newThreadCount = std::atoi(_parser.param(OPT_THREADS).c_str());
            if (newThreadCount > 0)
                _threadCount = newThreadCount;
        }
        if (_parser.isPresent(OPT_CHECKPOINTS))
            _checkpointInterval = StringUtils::parseDuration(_parser.param(OPT_CHECKPOINTS));
        if (_parser.isPresent(OPT_TIMEOUT))
            _timeout = StringUtils::parseDuration(_parser.param(OPT_TIMEOUT));

        embree::rtcInit();
        embree::rtcStartThreads(_threadCount);

        ThreadUtils::startThreads(_threadCount);

        if (_parser.isPresent(OPT_OUTPUT_DIRECTORY)) {
            _outputDirectory = Path(_parser.param(OPT_OUTPUT_DIRECTORY));
            _outputDirectory.freezeWorkingDirectory();
            _outputDirectory = _outputDirectory.absolute();
            if (!_outputDirectory.exists())
                FileUtils::createDirectory(_outputDirectory, true);
        }

        for (const std::string &p : _parser.operands())
            _status.queuedScenes.emplace_back(p);
    }

    bool renderScene()
    {
        Path currentScene;
        {
            std::unique_lock<std::mutex> lock(_statusMutex);
            if (_status.queuedScenes.empty())
                return false;

            _status.state = STATE_LOADING;
            _status.startSpp = _status.currentSpp = _status.nextSpp = _status.totalSpp = 0;

            currentScene = _status.currentScene = _status.queuedScenes.front();
            _status.queuedScenes.pop_front();
        }

        writeLogLine(tfm::format("Loading scene '%s'...", currentScene));
        try {
            std::unique_lock<std::mutex> lock(_sceneMutex);
            _scene.reset(Scene::load(Path(currentScene)));
            _scene->loadResources();
        } catch (std::runtime_error &e) {
            writeLogLine(tfm::format("Scene loader for file '%s' encountered an unrecoverable error: \n%s",
                    currentScene, e.what()));

            std::unique_lock<std::mutex> lock(_sceneMutex);
            _scene.reset();

            return true;
        }

        if (_parser.isPresent(OPT_SPP))
            _scene->rendererSettings().setSpp(std::atoi(_parser.param(OPT_SPP).c_str()));

        {
            std::unique_lock<std::mutex> lock(_statusMutex);
            _status.totalSpp = _scene->rendererSettings().spp();
        }

        try {
            DirectoryChange context(_scene->path().parent());

            if (_parser.isPresent(OPT_OUTPUT_DIRECTORY))
                _scene->rendererSettings().setOutputDirectory(_outputDirectory);

            uint32 seed = 0xBA5EBA11;
            if (_parser.isPresent(OPT_SEED))
                seed = std::atoi(_parser.param(OPT_SEED).c_str());

            int maxSpp = _scene->rendererSettings().spp();
            {
                std::unique_lock<std::mutex> lock(_sceneMutex);
                _flattenedScene.reset(_scene->makeTraceable(seed));
            }
            Integrator &integrator = _flattenedScene->integrator();

            if (!_parser.isPresent(OPT_CHECKPOINTS))
                _checkpointInterval = StringUtils::parseDuration(_scene->rendererSettings().checkpointInterval());
            if (!_parser.isPresent(OPT_TIMEOUT))
                _timeout = StringUtils::parseDuration(_scene->rendererSettings().timeout());

            if (_scene->rendererSettings().enableResumeRender() && !_parser.isPresent(OPT_RESTART)) {
                writeLogLine("Trying to resume render from saved state... ");
                if (integrator.resumeRender(*_scene))
                    writeLogLine("Resume successful");
                else
                    writeLogLine("Resume unsuccessful. Starting from 0 spp");
                {
                    std::unique_lock<std::mutex> lock(_statusMutex);
                    _status.startSpp = integrator.currentSpp();
                }
            }

            writeLogLine("Starting render...");
            Timer timer, checkpointTimer;
            double totalElapsed = 0.0;
            while (!integrator.done()) {
                {
                    std::unique_lock<std::mutex> lock(_statusMutex);
                    _status.state = STATE_RENDERING;
                    _status.currentSpp = integrator.currentSpp();
                    _status.nextSpp = integrator.nextSpp();
                }

                integrator.startRender([](){});
                integrator.waitForCompletion();
                writeLogLine(tfm::format("Completed %d/%d spp", integrator.currentSpp(), maxSpp));
                timer.stop();
                if (_timeout > 0.0 && timer.elapsed() > _timeout)
                    break;
                checkpointTimer.stop();
                if (_checkpointInterval > 0.0 && checkpointTimer.elapsed() > _checkpointInterval) {
                    totalElapsed += checkpointTimer.elapsed();
                    writeLogLine(tfm::format("Saving checkpoint after %s",
                            StringUtils::durationToString(totalElapsed)));
                    Timer ioTimer;
                    checkpointTimer.start();
                    integrator.saveCheckpoint();
                    if (_scene->rendererSettings().enableResumeRender())
                        integrator.saveRenderResumeData(*_scene);
                    ioTimer.stop();
                    writeLogLine(tfm::format("Saving checkpoint took %s",
                            StringUtils::durationToString(ioTimer.elapsed())));
                }
            }
            timer.stop();

            writeLogLine(tfm::format("Finished render. Render time %s",
                    StringUtils::durationToString(timer.elapsed())));

            integrator.saveOutputs();
            if (_scene->rendererSettings().enableResumeRender())
                integrator.saveRenderResumeData(*_scene);

            {
                std::unique_lock<std::mutex> lock(_statusMutex);
                _status.completedScenes.push_back(currentScene);
            }
        } catch (std::runtime_error &e) {
            writeLogLine(tfm::format("Renderer for file '%s' encountered an unrecoverable error: \n%s",
                    currentScene, e.what()));
        }

        {
            std::unique_lock<std::mutex> lock(_sceneMutex);
            _flattenedScene.reset();
            _scene.reset();
        }

        return true;
    }

    RendererStatus status()
    {
        std::unique_lock<std::mutex> lock(_statusMutex);
        RendererStatus copy(_status);
        return std::move(copy);
    }

    std::mutex &logMutex()
    {
        return _logMutex;
    }

    std::unique_ptr<Vec3c[]> frameBuffer(Vec2i &resolution)
    {
        std::unique_lock<std::mutex> lock(_sceneMutex);
        if (!_scene || !_flattenedScene)
            return nullptr;

        Vec2u res = _scene->camera()->resolution();
        std::unique_ptr<Vec3c[]> ldr(new Vec3c[res.product()]);

        for (uint32 y = 0; y < res.y(); ++y)
            for (uint32 x = 0; x < res.x(); ++x)
                ldr[x + y*res.x()] = Vec3c(clamp(Vec3i(_scene->camera()->get(x, y)*255.0f), Vec3i(0), Vec3i(255)));

        resolution = Vec2i(res);

        return std::move(ldr);
    }
};

}

#endif /* SHARED_HPP_ */

