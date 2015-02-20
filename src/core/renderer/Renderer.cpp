#include "Renderer.hpp"
#include "TraceableScene.hpp"

#include "integrators/Integrator.hpp"

#include "sampling/SobolSampler.hpp"

#include "cameras/Camera.hpp"

#include "thread/ThreadUtils.hpp"
#include "thread/ThreadPool.hpp"

#include "math/BitManip.hpp"

#include "io/FileUtils.hpp"
#include "io/ImageIO.hpp"
#include "io/Scene.hpp"

#include <rapidjson/stringbuffer.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <lodepng/lodepng.h>
#include <algorithm>

namespace Tungsten {

CONSTEXPR uint32 Renderer::TileSize;
CONSTEXPR uint32 Renderer::VarianceTileSize;
CONSTEXPR uint32 Renderer::AdaptiveThreshold;

static Path incrementalFilename(const Path &dstFile, const std::string &suffix, bool overwrite)
{
    Path dstPath = (dstFile.stripExtension() + suffix) + dstFile.extension();
    if (overwrite)
        return std::move(dstPath);

    Path barePath = dstPath.stripExtension();
    Path extension = dstPath.extension();

    int index = 0;
    while (dstPath.exists())
        dstPath = (barePath + tfm::format("%03d", ++index)) + extension;

    return std::move(dstPath);
}

Renderer::Renderer(TraceableScene &scene)
: _sampler(0xBA5EBA11),
  _scene(scene),
  _currentSpp(0)
{
    advanceSpp();

    for (uint32 i = 0; i < ThreadUtils::pool->threadCount(); ++i)
        _integrators.emplace_back(scene.cloneThreadSafeIntegrator(i));

    _w = scene.cam().resolution().x();
    _h = scene.cam().resolution().y();
    _varianceW = (_w + VarianceTileSize - 1)/VarianceTileSize;
    _varianceH = (_h + VarianceTileSize - 1)/VarianceTileSize;
    diceTiles();
    _samples.resize(_varianceW*_varianceH);
}

Renderer::~Renderer()
{
    abortRender();
}

void Renderer::diceTiles()
{
    for (uint32 y = 0; y < _h; y += TileSize) {
        for (uint32 x = 0; x < _w; x += TileSize) {
            _tiles.emplace_back(
                x,
                y,
                min(TileSize, _w - x),
                min(TileSize, _h - y),
                _scene.rendererSettings().useSobol() ?
                    std::unique_ptr<SampleGenerator>(new SobolSampler()) :
                    std::unique_ptr<SampleGenerator>(new UniformSampler(MathUtil::hash32(_sampler.nextI()))),
                std::unique_ptr<UniformSampler>(new UniformSampler(MathUtil::hash32(_sampler.nextI())))
            );
        }
    }
}

void Renderer::advanceSpp()
{
    _nextSpp = min(_currentSpp + _scene.rendererSettings().sppStep(), _scene.rendererSettings().spp());
}

float Renderer::errorPercentile95()
{
    std::vector<float> errors;
    errors.reserve(_samples.size());

    for (size_t i = 0; i < _samples.size(); ++i) {
        _samples[i].adaptiveWeight = _samples[i].errorEstimate();
        if (_samples[i].adaptiveWeight > 0.0f)
            errors.push_back(_samples[i].adaptiveWeight);
    }
    if (errors.empty())
        return 0.0f;
    std::sort(errors.begin(), errors.end());

    return errors[(errors.size()*95)/100];
}

void Renderer::dilateAdaptiveWeights()
{
    for (uint32 y = 0; y < _varianceH; ++y) {
        for (uint32 x = 0; x < _varianceW; ++x) {
            int idx = x + y*_varianceW;
            if (y < _varianceH - 1)
                _samples[idx].adaptiveWeight = max(_samples[idx].adaptiveWeight,
                        _samples[idx + _varianceW].adaptiveWeight);
            if (x < _varianceW - 1)
                _samples[idx].adaptiveWeight = max(_samples[idx].adaptiveWeight,
                        _samples[idx + 1].adaptiveWeight);
        }
    }
    for (int y = _varianceH - 1; y >= 0; --y) {
        for (int x = _varianceW - 1; x >= 0; --x) {
            int idx = x + y*_varianceW;
            if (y > 0)
                _samples[idx].adaptiveWeight = max(_samples[idx].adaptiveWeight,
                        _samples[idx - _varianceW].adaptiveWeight);
            if (x > 0)
                _samples[idx].adaptiveWeight = max(_samples[idx].adaptiveWeight,
                        _samples[idx - 1].adaptiveWeight);
        }
    }
}

void Renderer::distributeAdaptiveSamples(int spp)
{
    double totalWeight = 0.0;
    for (SampleRecord &record : _samples)
        totalWeight += record.adaptiveWeight;

    int adaptiveBudget = (spp - 1)*_w*_h;
    int budgetPerTile = adaptiveBudget/(VarianceTileSize*VarianceTileSize);
    float weightToSampleFactor = double(budgetPerTile)/totalWeight;

    float pixelPdf = 0.0f;
    for (SampleRecord &record : _samples) {
        float fractionalSamples = record.adaptiveWeight*weightToSampleFactor;
        int adaptiveSamples = int(fractionalSamples);
        pixelPdf += fractionalSamples - float(adaptiveSamples);
        if (_sampler.next1D() < pixelPdf) {
            adaptiveSamples++;
            pixelPdf -= 1.0f;
        }
        record.nextSampleCount = adaptiveSamples + 1;
    }
}

bool Renderer::generateWork()
{
    for (SampleRecord &record : _samples)
        record.sampleIndex += record.nextSampleCount;

    int sppCount = _nextSpp - _currentSpp;
    bool enableAdaptive = _scene.rendererSettings().useAdaptiveSampling();

    if (enableAdaptive && _currentSpp >= AdaptiveThreshold) {
        float maxError = errorPercentile95();
        if (maxError == 0.0f)
            return false;

        for (SampleRecord &record : _samples)
            record.adaptiveWeight = min(record.adaptiveWeight, maxError);

        dilateAdaptiveWeights();
        distributeAdaptiveSamples(sppCount);
    } else {
        for (SampleRecord &record : _samples)
            record.nextSampleCount = sppCount;
    }

    return true;
}

void Renderer::renderTile(uint32 id, uint32 tileId)
{
    ImageTile &tile = _tiles[tileId];
    for (uint32 y = 0; y < tile.h; ++y) {
        for (uint32 x = 0; x < tile.w; ++x) {
            Vec2u pixel(tile.x + x, tile.y + y);
            uint32 pixelIndex = pixel.x() + pixel.y()*_w;
            uint32 variancePixelIndex = pixel.x()/VarianceTileSize + pixel.y()/VarianceTileSize*_varianceW;

            SampleRecord &record = _samples[variancePixelIndex];
            int spp = record.nextSampleCount;
            Vec3f c(0.0f);
            for (int i = 0; i < spp; ++i) {
                tile.sampler->setup(pixelIndex, record.sampleIndex + i);
                Vec3f s(_integrators[id]->traceSample(pixel, *tile.sampler, *tile.supplementalSampler));

                record.addSample(s);
                c += s;
            }

            _scene.cam().addSamples(x + tile.x, y + tile.y, c, spp);
        }
    }
}

void Renderer::writeBuffers(const std::string &suffix, bool overwrite)
{
    Vec2u res = _scene.cam().resolution();
    std::unique_ptr<Vec3f[]> hdr(new Vec3f[res.product()]);
    std::unique_ptr<Vec3c[]> ldr(new Vec3c[res.product()]);

    for (uint32 y = 0; y < res.y(); ++y) {
        for (uint32 x = 0; x < res.x(); ++x) {
            hdr[x + y*res.x()] = _scene.cam().getLinear(x, y);
            ldr[x + y*res.x()] = Vec3c(clamp(Vec3i(_scene.cam().get(x, y)*255.0f), Vec3i(0), Vec3i(255)));
        }
    }

    const RendererSettings &settings = _scene.rendererSettings();

    if (!settings.outputFile().empty())
        ImageIO::saveLdr(incrementalFilename(settings.outputFile(), suffix, overwrite),
                &ldr[0].x(), res.x(), res.y(), 3);
    if (!settings.hdrOutputFile().empty())
        ImageIO::saveHdr(incrementalFilename(settings.hdrOutputFile(), suffix, overwrite),
                &hdr[0].x(), res.x(), res.y(), 3);

    if (!settings.varianceOutputFile().empty()) {
        std::unique_ptr<uint8[]> ldrVariance(new uint8[_varianceW*_varianceH]);

        float maxError = max(errorPercentile95(), 1e-5f);
        for (size_t i = 0; i < _samples.size(); ++i)
            ldrVariance[i] = int(clamp(255.0f*_samples[i].errorEstimate()/maxError, 0.0f, 255.0f));

        ImageIO::saveLdr(incrementalFilename(settings.varianceOutputFile(), suffix, overwrite),
                ldrVariance.get(), _varianceW, _varianceH, 1);
    }
}

void Renderer::startRender(std::function<void()> completionCallback)
{
    if (done() || !generateWork()) {
        _currentSpp = _nextSpp;
        advanceSpp();
        completionCallback();
        return;
    }

    using namespace std::placeholders;
    _group = ThreadUtils::pool->enqueue(
        std::bind(&Renderer::renderTile, this, _3, _1),
        _tiles.size(),
        [&, completionCallback]() {
            _currentSpp = _nextSpp;
            advanceSpp();
            completionCallback();
        }
    );
}

void Renderer::waitForCompletion()
{
    if (_group)
        _group->wait();
}

void Renderer::abortRender()
{
    if (_group) {
        _group->abort();
        _group->wait();
    }
}

void Renderer::saveOutputs()
{
    writeBuffers("", _scene.rendererSettings().overwriteOutputFiles());
}

void Renderer::saveCheckpoint()
{
    writeBuffers("_checkpoint", true);
}

// Computes a hash of everything in the scene except the renderer settings
// This is done by serializing everything to JSON and hashing the resulting string
static uint64 sceneHash(Scene &scene)
{
    rapidjson::Document document;
    document.SetObject();
    *(static_cast<rapidjson::Value *>(&document)) = scene.toJson(document.GetAllocator());
    document.RemoveMember("renderer");

    rapidjson::GenericStringBuffer<rapidjson::UTF8<>> buffer;
    rapidjson::Writer<rapidjson::GenericStringBuffer<rapidjson::UTF8<>>> jsonWriter(buffer);
    document.Accept(jsonWriter);

    return BitManip::hash(buffer.GetString());
}

void Renderer::saveRenderResumeData(Scene &scene)
{
    rapidjson::Document document;
    document.SetObject();
    document.AddMember("current_spp", _currentSpp, document.GetAllocator());
    document.AddMember("adaptive_sampling", _scene.rendererSettings().useAdaptiveSampling(), document.GetAllocator());
    document.AddMember("stratified_sampler", _scene.rendererSettings().useSobol(), document.GetAllocator());
    if (!FileUtils::writeJson(document, _scene.rendererSettings().resumeRenderPrefix() + ".json")) {
        DBG("Failed to write render resume state JSON");
        return;
    }

    OutputStreamHandle out = FileUtils::openOutputStream(_scene.rendererSettings().resumeRenderPrefix() + ".dat");
    if (!out) {
        DBG("Failed to open render resume state data stream");
        return;
    }

    uint64 jsonHash = sceneHash(scene);
    FileUtils::streamWrite(out, _currentSpp);
    FileUtils::streamWrite(out, jsonHash);
    FileUtils::streamWrite(out, _scene.cam().pixels());
    FileUtils::streamWrite(out, _scene.cam().weights());
    for (SampleRecord &s : _samples)
        s.saveState(out);
    for (ImageTile &i : _tiles) {
        i.sampler->saveState(out);
        i.supplementalSampler->saveState(out);
    }
}

bool Renderer::resumeRender(Scene &scene)
{
    std::string json = FileUtils::loadText(_scene.rendererSettings().resumeRenderPrefix() + ".json");
    if (json.empty())
        return false;

    rapidjson::Document document;
    document.Parse<0>(json.c_str());
    if (document.HasParseError() || !document.IsObject())
        return false;

    bool adaptiveSampling, stratifiedSampler;
    if (!JsonUtils::fromJson(document, "adaptive_sampling", adaptiveSampling)
            || adaptiveSampling != _scene.rendererSettings().useAdaptiveSampling())
        return false;
    if (!JsonUtils::fromJson(document, "stratified_sampler", stratifiedSampler)
            || stratifiedSampler != _scene.rendererSettings().useSobol())
        return false;

    InputStreamHandle in = FileUtils::openInputStream(_scene.rendererSettings().resumeRenderPrefix() + ".dat");
    if (!in)
        return false;

    uint32 dataSpp;
    FileUtils::streamRead(in, dataSpp);

    uint32 jsonSpp;
    if (!JsonUtils::fromJson(document, "current_spp", jsonSpp) || jsonSpp != dataSpp)
        return false;

    uint64 jsonHash;
    FileUtils::streamRead(in, jsonHash);
    if (jsonHash != sceneHash(scene))
        return false;

    FileUtils::streamRead(in, _scene.cam().pixels());
    FileUtils::streamRead(in, _scene.cam().weights());
    for (SampleRecord &s : _samples)
        s.loadState(in);
    for (ImageTile &i : _tiles) {
        i.sampler->loadState(in);
        i.supplementalSampler->loadState(in);
    }

    _currentSpp = dataSpp;
    advanceSpp();

    return true;
}

}
