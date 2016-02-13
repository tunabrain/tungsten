#include "Version.hpp"

#include "thread/ThreadUtils.hpp"

#include "io/CliParser.hpp"
#include "io/ImageIO.hpp"
#include "io/Scene.hpp"

#include "Timer.hpp"

#include <tinyformat/tinyformat.hpp>
#include <cstdlib>
#include <cstring>

using namespace Tungsten;

static const int OPT_VERSION  = 0;
static const int OPT_HELP     = 1;
static const int OPT_FWEIGHT  = 2;
static const int OPT_CWEIGHT  = 3;

void normalize(Vec3f *img, float *imgV, int w, int h)
{
    Vec3f maximum(0.0f);
    for (int i = 0; i < w*h; ++i)
        maximum = max(maximum, img[i]);
    for (int i = 0; i < w*h; ++i) {
        img [i] /= maximum;
        imgV[i] /= sqr(maximum.avg());
    }
}

std::unique_ptr<Vec3f[]> floatArrayToVecArray(std::unique_ptr<float[]> img, int w, int h)
{
    std::unique_ptr<Vec3f[]> result(new Vec3f[w*h]);
    std::memcpy(result.get(), img.get(), w*h*sizeof(Vec3f));
    return std::move(result);
}

void saveVecImage(const Vec3f *img, Path path, int w, int h)
{
    ImageIO::saveHdr(path, img[0].data(), w, h, 3);
}

std::unique_ptr<Vec3f[]> loadVecImage(Path path, int &w, int &h)
{
    auto img = ImageIO::loadHdr(path, TexelConversion::REQUEST_RGB, w, h);
    if (!img)
        return nullptr;
    return floatArrayToVecArray(std::move(img), w, h);
}

std::unique_ptr<float[]> loadFloatImage(Path path, int &w, int &h)
{
    return ImageIO::loadHdr(path, TexelConversion::REQUEST_AVERAGE, w, h);
}

template<typename Distance>
std::unique_ptr<Vec3f[]> nlMeansFilter(std::unique_ptr<Vec3f[]> img, float *var,
        int w, int h, float kC, Distance distance)
{
    CONSTEXPR int F = 3;
    CONSTEXPR int R = 5;

    auto delta = [&](int idxP, int idxQ) {
        float v = var ? var[idxP] + var[idxQ] : 1.0f;
        return sqr(img[idxP] - img[idxQ])/(1e-3f + kC*kC*v);
    };
    auto patchDistance = [&](int xp, int yp, int xq, int yq) {
        float weight = 0.0f;
        float dist = 0.0f;
        for (int dy = -F; dy <= F; ++dy) {
            for (int dx = -F; dx <= F; ++dx) {
                int xpi = xp + dx, ypi = yp + dy;
                int xqi = xq + dx, yqi = yq + dy;
                if (xpi <  0 || xqi <  0 || ypi <  0 || yqi <  0 ||
                    xpi >= w || xqi >= w || ypi >= h || yqi >= h)
                    continue;
                dist += delta(xpi + ypi*w, xqi + yqi*w).sum();
                weight += 3.0f;
            }
        }
        return dist/weight;
    };

    std::unique_ptr<Vec3f[]> result(new Vec3f[w*h]);
    ThreadUtils::parallelFor(0, h, 20, [&](Tungsten::uint32 y) {
        for (int x = 0; x < w; ++x) {
            float weightSum = 0.0f;
            Vec3f filtered(0.0f);
            for (int t = -R; t <= R; ++t) {
                for (int s = -R; s <= R; ++s) {
                    int xq = x + s, yq = y + t;
                    if (xq < 0 || yq < 0 || xq >= w || yq >= h)
                        continue;
                    float wc = std::exp(-patchDistance(x, y, xq, yq));
                    float wi = distance(wc, x + y*w, xq + yq*w);
                    filtered += wi*img[xq + yq*w];
                    weightSum += wi;
                }
            }
            result[x + y*w] = filtered/weightSum;
        }
    });

    return std::move(result);
}

int main(int argc, const char *argv[])
 {
     CliParser parser("denoiser", "[options] scene outputfile");
     parser.addOption('h', "help", "Prints this help text", false, OPT_HELP);
     parser.addOption('v', "version", "Prints version information", false, OPT_VERSION);
     parser.addOption('f', "fweight", "Specify feature weight (default: 1)", true, OPT_FWEIGHT);
     parser.addOption('c', "cweight", "Specify color buffer weight (default: 1)", true, OPT_CWEIGHT);

     parser.parse(argc, argv);

     if (parser.operands().size() != 2 || parser.isPresent(OPT_HELP)) {
         parser.printHelpText();
         return 0;
     }
     if (parser.isPresent(OPT_VERSION)) {
         std::cout << "denoiser, version " << VERSION_STRING << std::endl;
         return 0;
     }

     Path sceneFile(parser.operands()[0]);
     Path targetFile(parser.operands()[1]);

     std::cout << tfm::format("Loading scene '%s'...", sceneFile) << std::endl;

     std::unique_ptr<Scene> scene;
     try {
         scene.reset(Scene::load(sceneFile));
     } catch (std::runtime_error &e) {
         std::cerr << tfm::format("Scene loader for file '%s' encountered an unrecoverable error: \n%s",
                 sceneFile, e.what()) << std::endl;

         return 1;
     }

    ThreadUtils::startThreads(max(ThreadUtils::idealThreadCount() - 1, 1u));

    Path hdrImage = scene->rendererSettings().hdrOutputFile();
    if (hdrImage.empty()) {
        std::cerr << "Can only filter HDR images. "
                "Please remember to set hdr_output_file in the renderer settings" << std::endl;
        return 1;
    }


    int w, h;
    std::unique_ptr<Vec3f[]> image = loadVecImage(hdrImage, w, h);
    if (!image) {
        std::cerr << tfm::format("Failed to load HDR image at '%s'", hdrImage) << std::endl;
        return 1;
    }
    std::unique_ptr<float[]> imageVariance;

    std::vector<std::unique_ptr<Vec3f[]>> features;
    std::vector<std::unique_ptr<float[]>> featureVariance;
    for (const auto &b : scene->rendererSettings().renderOutputs()) {
        if (!b.hdrOutputFile().empty()) {
            Path file = b.hdrOutputFile();

            if (b.type() == OutputColor) {
                if (b.sampleVariance()) {
                    Path varianceFile = file.stripExtension() + "Variance" + file.extension();
                    imageVariance = loadFloatImage(varianceFile, w, h);
                }
            } else {
                std::unique_ptr<Vec3f[]> feature = loadVecImage(file, w, h);
                if (feature) {
                    features.emplace_back(std::move(feature));

                    if (b.sampleVariance()) {
                        Path varianceFile = file.stripExtension() + "Variance" + file.extension();
                        featureVariance.emplace_back(loadFloatImage(varianceFile, w, h));
                    }

                    std::cout << "Using feature " << b.typeString() << std::endl;
                }
            }
        }
    }

    float kF = 1.0f;
    if (parser.isPresent(OPT_FWEIGHT)) {
        float f = std::atof(parser.param(OPT_FWEIGHT).c_str());
        if (f > 0.0f)
            kF = f;
    }
    float kC = 1.0f;
    if (parser.isPresent(OPT_CWEIGHT)) {
        float f = std::atof(parser.param(OPT_CWEIGHT).c_str());
        if (f > 0.0f)
            kC = f;
    }

    const float Tau = 1e-3f;

    auto bilateralWeight = [&](const Vec3f *f, const float *var, int idxP, int idxQ) {
        float v = var ? var[idxP] : 1.0f;
        return (sqr(f[idxP] - f[idxQ])/(sqr(kF*0.6f)*max(Tau, v))).sum();
    };

    Timer timer;
    image = nlMeansFilter(std::move(image), imageVariance.get(), w, h, kC, [&](float wc, int idxP, int idxQ) {
        float dF = 0.0f;
        for (size_t i = 0; i < features.size(); ++i)
            dF = max(dF, bilateralWeight(features[i].get(), featureVariance[i].get(), idxP, idxQ));
        float wf = std::exp(-dF);
        return min(wf, wc);
    });
    timer.bench("Filtering took");

    saveVecImage(image.get(), targetFile, w, h);

    return 0;
}
