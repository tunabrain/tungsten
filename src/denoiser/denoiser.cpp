#include "Version.hpp"

#include "thread/ThreadUtils.hpp"

#include "io/JsonLoadException.hpp"
#include "io/CliParser.hpp"
#include "io/ImageIO.hpp"
#include "io/Scene.hpp"

#include "Regression.hpp"
#include "NlMeans.hpp"
#include "Pixmap.hpp"

#include "Logging.hpp"
#include "Memory.hpp"
#include "Timer.hpp"

#include <tinyformat/tinyformat.hpp>
#include <cstdlib>
#include <cstring>

using namespace Tungsten;

static const int OPT_VERSION  = 0;
static const int OPT_HELP     = 1;

template<typename Texel>
struct RenderBuffer
{
    std::unique_ptr<Pixmap<Texel>> buffer;
    std::unique_ptr<Pixmap<Texel>> bufferA;
    std::unique_ptr<Pixmap<Texel>> bufferB;
    std::unique_ptr<Pixmap<Texel>> bufferVariance;
};
typedef RenderBuffer<float> RenderBufferF;
typedef RenderBuffer<Vec3f> RenderBuffer3f;

Pixmap3f nforDenoiser(RenderBuffer3f image, std::vector<RenderBufferF> features)
{
    int w = image.buffer->w(), h = image.buffer->h();

    // Feature cross-prefiltering (section 5.1)
    printTimestampedLog("Prefiltering features...");
    std::vector<PixmapF> filteredFeaturesA(features.size());
    std::vector<PixmapF> filteredFeaturesB(features.size());
    SimdNlMeans featureFilter;
    for (size_t i = 0; i < features.size(); ++i) {
        featureFilter.addBuffer(filteredFeaturesA[i], *features[i].bufferA, *features[i].bufferB, *features[i].bufferVariance);
        featureFilter.addBuffer(filteredFeaturesB[i], *features[i].bufferB, *features[i].bufferA, *features[i].bufferVariance);
    }
    featureFilter.denoise(3, 5, 0.5f, 2.0f);
    features.clear();
    printTimestampedLog("Prefiltering done");

    // Main regression (section 5.2)
    std::vector<Pixmap3f> filteredColorsA;
    std::vector<Pixmap3f> filteredColorsB;
    std::vector<Pixmap3f> mses;
    for (float k : {0.5f, 1.0f}) {
        printTimestampedLog(tfm::format("Beginning regression pass %d/2", mses.size() + 1));
        // Regression pass
        printTimestampedLog("Denosing half buffer A...");
        Pixmap3f filteredColorA = collaborativeRegression(*image.bufferA, *image.bufferB, filteredFeaturesB, *image.bufferVariance, 3, 9, k);
        printTimestampedLog("Denosing half buffer B...");
        Pixmap3f filteredColorB = collaborativeRegression(*image.bufferB, *image.bufferA, filteredFeaturesA, *image.bufferVariance, 3, 9, k);

        // MSE estimation (section 5.3)
        printTimestampedLog("Estimating MSE...");
        Pixmap3f noisyMse(w, h);
        for (int i = 0; i < w*h; ++i) {
            Vec3f mseA = sqr((*image.bufferB)[i] - filteredColorA[i]) - 2.0f*(*image.bufferVariance)[i];
            Vec3f mseB = sqr((*image.bufferA)[i] - filteredColorB[i]) - 2.0f*(*image.bufferVariance)[i];
            Vec3f residualColorVariance = sqr(filteredColorB[i] - filteredColorA[i])*0.25f;

            noisyMse[i] = (mseA + mseB)*0.5f - residualColorVariance;
        }
        filteredColorsA.emplace_back(std::move(filteredColorA));
        filteredColorsB.emplace_back(std::move(filteredColorB));

        // MSE filtering
        mses.emplace_back(nlMeans(noisyMse, *image.buffer, *image.bufferVariance, 1, 9, 1.0f, 1.0f, true));
    }
    printTimestampedLog("Regression pass done");

    // Bandwidth selection (section 5.3)
    // Generate selection map
    printTimestampedLog("Generating selection maps...");
    Pixmap3f noisySelection(w, h);
    for (int i = 0; i < w*h; ++i)
        for (int j = 0; j < 3; ++j)
            noisySelection[i][j] = mses[0][i][j] < mses[1][i][j] ? 0.0f : 1.0f;
    mses.clear();
    // Filter selection map
    Pixmap3f selection = nlMeans(noisySelection, *image.buffer, *image.bufferVariance, 1, 9, 1.0f, 1.0f, true);

    // Apply selection map
    Pixmap3f resultA(w, h);
    Pixmap3f resultB(w, h);
    for (int i = 0; i < w*h; ++i) {
        resultA[i] += lerp(filteredColorsA[0][i], filteredColorsA[1][i], selection[i]);
        resultB[i] += lerp(filteredColorsB[0][i], filteredColorsB[1][i], selection[i]);
    }
    selection.reset();
    filteredColorsA.clear();
    filteredColorsB.clear();

    // Second filter pass (section 5.4)
    printTimestampedLog("Beginning second filter pass");
    printTimestampedLog("Denoising final features...");
    std::vector<PixmapF> finalFeatures;
    for (size_t i = 0; i < filteredFeaturesA.size(); ++i) {
        PixmapF combinedFeature(w, h);
        PixmapF combinedFeatureVar(w, h);

        for (int j = 0; j < w*h; ++j) {
            combinedFeature   [j] =    (filteredFeaturesA[i][j] + filteredFeaturesB[i][j])*0.5f;
            combinedFeatureVar[j] = sqr(filteredFeaturesB[i][j] - filteredFeaturesA[i][j])*0.25f;
        }
        filteredFeaturesA[i].reset();
        filteredFeaturesB[i].reset();

        finalFeatures.emplace_back(nlMeans(combinedFeature, combinedFeature, combinedFeatureVar, 3, 2, 0.5f));
    }

    Pixmap3f combinedResult(w, h);
    Pixmap3f combinedResultVar(w, h);
    for (int j = 0; j < w*h; ++j) {
        combinedResult   [j] =    (resultA[j] + resultB[j])*0.5f;
        combinedResultVar[j] = sqr(resultB[j] - resultA[j])*0.25f;
    }
    printTimestampedLog("Performing final regression...");
    return collaborativeRegression(combinedResult, combinedResult, finalFeatures, combinedResultVar, 3, 9, 1.0f);
}

// Extracts a single channel of an RGB image into a separate pixmap
std::unique_ptr<PixmapF> slicePixmap(const Pixmap3f &src, int channel)
{
    int w = src.w(), h = src.h();

    auto result = std::unique_ptr<PixmapF>(new PixmapF(w, h));
    for (int j = 0; j < w*h; ++j)
        (*result)[j] = src[j][channel];

    return std::move(result);
}

void loadInputBuffers(RenderBuffer3f &image, std::vector<RenderBufferF> &features, const Scene &scene)
{
    for (const auto &b : scene.rendererSettings().renderOutputs()) {
        if (!b.hdrOutputFile().empty()) {
            Path file = b.hdrOutputFile();
            auto buffer = loadPixmap<Vec3f>(file, true);
            if (buffer) {
                std::unique_ptr<Pixmap3f> bufferVariance;
                if (b.sampleVariance()) {
                    Path varianceFile = file.stripExtension() + "Variance" + file.extension();
                    bufferVariance = loadPixmap<Vec3f>(varianceFile);
                }
                std::unique_ptr<Pixmap3f> bufferA, bufferB;
                if (b.twoBufferVariance()) {
                    Path fileA = file.stripExtension() + "A" + file.extension();
                    Path fileB = file.stripExtension() + "B" + file.extension();
                    bufferA = loadPixmap<Vec3f>(fileA);
                    bufferB = loadPixmap<Vec3f>(fileB);
                }

                if (b.type() == OutputColor) {
                    image.buffer         = std::move(buffer);
                    image.bufferA        = std::move(bufferA);
                    image.bufferB        = std::move(bufferB);
                    image.bufferVariance = std::move(bufferVariance);
                } else {
                    bool isRgb = (b.type() == OutputNormal || b.type() == OutputAlbedo);
                    for (int i = 0; i < (isRgb ? 3 : 1); ++i) {
                        features.emplace_back();
                        if (buffer        ) features.back().buffer         = slicePixmap(*buffer        , i);
                        if (bufferA       ) features.back().bufferA        = slicePixmap(*bufferA       , i);
                        if (bufferB       ) features.back().bufferB        = slicePixmap(*bufferB       , i);
                        if (bufferVariance) features.back().bufferVariance = slicePixmap(*bufferVariance, i);
                    }
                    printTimestampedLog(tfm::format("Using feature %s", b.typeString()));
                }
            }
        }
    }
}

int main(int argc, const char *argv[])
{
    CliParser parser("denoiser", "[options] scene outputfile");
    parser.addOption('h', "help", "Prints this help text", false, OPT_HELP);
    parser.addOption('v', "version", "Prints version information", false, OPT_VERSION);

    parser.parse(argc, argv);
    if (parser.isPresent(OPT_VERSION)) {
        std::cout << "denoiser, version " << VERSION_STRING << std::endl;
        return 0;
    }
    if (parser.operands().size() != 2 || parser.isPresent(OPT_HELP)) {
        parser.printHelpText();
        return 0;
    }

    Path sceneFile(parser.operands()[0]);
    Path targetFile(parser.operands()[1]);

    printTimestampedLog(tfm::format("Loading scene '%s'...", sceneFile));

    std::unique_ptr<Scene> scene;
    try {
        scene.reset(Scene::load(sceneFile));
    } catch (const JsonLoadException &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    ThreadUtils::startThreads(max(ThreadUtils::idealThreadCount() - 1, 1u));

    RenderBuffer3f image;
    std::vector<RenderBufferF> features;
    loadInputBuffers(image, features, *scene);

    Timer timer;
    Pixmap3f result = nforDenoiser(std::move(image), std::move(features));
    timer.stop();
    printTimestampedLog(tfm::format("Filtering complete! Filter time: %.1fs", timer.elapsed()));

    result.save(targetFile, true);

    return 0;
}
