#include "Version.hpp"

#include "cameras/Tonemap.hpp"

#include "thread/ThreadUtils.hpp"
#include "thread/ThreadPool.hpp"

#include "io/FileUtils.hpp"
#include "io/CliParser.hpp"
#include "io/ImageIO.hpp"
#include "io/Path.hpp"

#include "Memory.hpp"

#include <iostream>
#include <cstdlib>

using namespace Tungsten;

static const int OPT_OUTPUT            = 1;
static const int OPT_VERSION           = 2;
static const int OPT_HELP              = 3;
static const int OPT_MERGE             = 4;
static const int OPT_EXPOSURE          = 5;
static const int OPT_TONEMAP           = 6;
static const int OPT_FILETYPE          = 7;
static const int OPT_AVG               = 8;
static const int OPT_WEIGHTS           = 9;
static const int OPT_MSE               = 10;
static const int OPT_RMSE              = 11;
static const int OPT_MSEMAP            = 12;
static const int OPT_RMSEMAP           = 13;
static const int OPT_VARIANCE          = 14;

void parseFloat(float &dst, const std::string &src)
{
    const char *srcBuf = src.c_str();
    char *end;
    float result = std::strtof(srcBuf, &end);
    if (end != srcBuf)
        dst = result;
}

void outputImage(CliParser &parser, Path path, std::unique_ptr<float[]> img, int w, int h,
        float exposure, Tonemap::Type tonemap)
{
    if (exposure != 0.0f) {
        float scale = std::pow(2.0f, exposure);
        for (int i = 0; i < w*h*3; ++i)
            img[i] *= scale;
    }

    if (path.testExtension("png")) {
        std::unique_ptr<uint8[]> ldr(new uint8[w*h*3]);
        for (int i = 0; i < w*h; ++i) {
            Vec3f c = Tonemap::tonemap(tonemap, Vec3f(img[i*3], img[i*3 + 1], img[i*3 + 2]));
            ldr[i*3 + 0] = clamp(int(c.x()*255.0f), 0, 255);
            ldr[i*3 + 1] = clamp(int(c.y()*255.0f), 0, 255);
            ldr[i*3 + 2] = clamp(int(c.z()*255.0f), 0, 255);
        }
        if (!ImageIO::saveLdr(path, ldr.get(), w, h, 3))
            parser.fail("Unable to write output file '%s'", path);
    } else {
        if (!ImageIO::saveHdr(path, img.get(), w, h, 3))
            parser.fail("Unable to write output file '%s'", path);
    }
}

void mergeImages(CliParser &parser, Path path, const std::vector<std::string> &operands,
        const std::vector<double> weights, float exposure, Tonemap::Type tonemap)
{
    int resultW = 0, resultH = 0;
    double weightSum = 0.0;
    std::unique_ptr<double[]> result;
    for (size_t i = 0; i < operands.size(); ++i) {
        int w, h;
        std::unique_ptr<float[]> operand = ImageIO::loadHdr(Path(operands[i]),
                TexelConversion::REQUEST_RGB, w, h);

        if (!operand)
            parser.fail("Unable to load input file at '%s'", operands[i]);

        if (!result) {
            resultW = w;
            resultH = h;
            result.reset(new double[w*h*3]);
            std::memset(result.get(), 0, sizeof(double)*w*h*3);
        }
        if (w != resultW || h != resultH)
            std::cout << "Warning: Image " << operands[i] << " has wrong dimensions ("
                      << w << "x" << h << "). Merged image has dimensions "
                      << resultW << "x" << resultH << ". "
                      << "hdrmanip will try to do what it can."
                      << std::endl;

        int dimX = min(resultW, w);
        int dimY = min(resultH, h);

        for (int y = 0; y < dimY; ++y)
            for (int x = 0; x < dimX; ++x)
                for (int c = 0; c < 3; ++c)
                    result[(x + y*resultW)*3 + c] += weights[i]*operand[(x + y*w)*3 + c];

        weightSum += weights[i];
    }

    std::unique_ptr<float[]> img(new float[resultW*resultH*3]);
    for (int i = 0; i < resultW*resultH*3; ++i)
        img[i] = float(result[i]/weightSum);

    outputImage(parser, path, std::move(img), resultW, resultH, exposure, tonemap);
}

std::unique_ptr<float[]> mseMap(int w, int h, std::unique_ptr<float[]> imgA, std::unique_ptr<float[]> imgB)
{
    std::unique_ptr<float[]> result(new float[w*h]);

    int maxX = 0, maxY = 0;
    float maxMse = 0.0f;
    for (int i = 0; i < w*h; ++i) {
        float mse = 0.0f;
        for (int c = 0; c < 3; ++c)
            mse += sqr(imgA[i*3 + c] - imgB[i*3 + c]);
        if (mse > maxMse) {
            maxMse = mse;
            maxX = i % w;
            maxY = i/w;
        }
        result[i] = mse/3.0f;
    }

    Vec3f *a = reinterpret_cast<Vec3f *>(imgA.get());
    Vec3f *b = reinterpret_cast<Vec3f *>(imgB.get());

    std::cout << maxX << " " << maxY << " " << maxMse << " " << a[maxX + maxY*w] << " " << b[maxX + maxY*w] << std::endl;

    return std::move(result);
}

std::unique_ptr<float[]> rmseMap(int w, int h, std::unique_ptr<float[]> imgA, std::unique_ptr<float[]> imgB)
{
    std::unique_ptr<float[]> result(new float[w*h]);

    for (int i = 0; i < w*h; ++i) {
        float rmse = 0.0f;
        for (int c = 0; c < 3; ++c)
            rmse += sqr(imgA[i*3 + c] - imgB[i*3 + c])/(sqr(imgA[i*3 + c]) + 1e-3f);
        result[i] = rmse/3.0f;
    }

    return std::move(result);
}

Vec3f colorRamp(float t)
{
    Vec3f ramp[] = {
        Vec3f(0.0f, 0.0f, 1.0f),
        Vec3f(0.0f, 1.0f, 1.0f),
        Vec3f(0.0f, 1.0f, 0.0f),
        Vec3f(1.0f, 1.0f, 0.0f),
        Vec3f(1.0f, 0.0f, 0.0f),
    };
    int l = clamp(int(t*4.0f), 0, 3);
    return lerp(ramp[l], ramp[l + 1], t*4.0f - l);
}

std::unique_ptr<float[]> heatMap(const float *in, int w, int h, int percentile)
{
    std::unique_ptr<float[]> img(new float[w*h*3]);

    float minPixel = 0.0f;
    for (int i = 0; i < w*h; ++i)
        minPixel = min(minPixel, in[i]);

    double mseSum = 0.0;
    for (int i = 0; i < w*h; ++i)
        mseSum += in[i];

    std::unique_ptr<float[]> sorted(new float[w*h]);
    std::memcpy(sorted.get(), in, w*h*sizeof(float));
    std::sort(sorted.get(), sorted.get() + w*h);

    double tailEnd = 0.0;
    uint32 tail = w*h - 1;
    while (tailEnd/mseSum < 0.8f)
        tailEnd += sorted[tail--];

    float maxPixel = sorted[min((w*h*percentile)/100, w*h - 1)];
    if (percentile == 100) {
        //maxPixel = 1.0f;
        maxPixel = sorted[tail];
        minPixel = 0.0f;
    }

    for (int i = 0; i < w*h; ++i)
        reinterpret_cast<Vec3f *>(img.get())[i] = colorRamp(clamp((in[i] - minPixel)/(maxPixel - minPixel), 0.0f, 1.0f));

    return std::move(img);
}

int main(int argc, const char *argv[])
{
    CliParser parser("hdrmanip", "[options] file1 [file2 [file3 ....]]");
    parser.addOption('h', "help", "Prints this help text", false, OPT_HELP);
    parser.addOption('v', "version", "Prints version information", false, OPT_VERSION);
    parser.addOption('o', "output", "Specifies the output file or directory", true, OPT_OUTPUT);
    parser.addOption('m', "merge", "Merges input files into one by averaging them", false, OPT_MERGE);
    parser.addOption('w', "weights", "Specifies comma separated list of weights when using merge", true, OPT_WEIGHTS);
    parser.addOption('e', "exposure", "Specifies the exposure to apply to the input image "
            "(default: 0)", true, OPT_EXPOSURE);
    parser.addOption('a', "average", "Computes average image value", false, OPT_AVG);
    parser.addOption('t', "tonemap", "Specifies the tonemapping operator to apply when "
            "converting to low dynamic range. Available options: linear, gamma, reinhard, filmic. "
            " (default: gamma)", true, OPT_TONEMAP);
    parser.addOption('f', "filetype", "When converting multiple images, specifies the file type to "
            "save the results as. Available options: "
#if OPENEXR_AVAILABLE
            "exr, "
#endif
            "png, pfm.", true, OPT_FILETYPE);
    parser.addOption('\0', "mse", "Computes mean square error of two input images", false, OPT_MSE);
    parser.addOption('\0', "rmse", "Computes relative mean square error of two input images", false, OPT_RMSE);
    parser.addOption('\0', "mse-map", "Computes heat map of the mean square error of two input images", false, OPT_MSEMAP);
    parser.addOption('\0', "rmse-map", "Computes heat map of the relative mean square error of two input images", false, OPT_RMSEMAP);
    parser.addOption('\0', "variance", "Compute sample variance of input images", false, OPT_VARIANCE);

    parser.parse(argc, argv);

    if (argc < 2 || parser.isPresent(OPT_HELP)) {
        parser.printHelpText();
        return 0;
    }
    if (parser.isPresent(OPT_VERSION)) {
        std::cout << "hdrmanip, version " << VERSION_STRING << std::endl;
        return 0;
    }
    if (parser.operands().empty())
        parser.fail("No input files");

    ThreadUtils::startThreads(max(ThreadUtils::idealThreadCount() - 1, 1u));

    const std::vector<std::string> &operands = parser.operands();

    bool isOutputDirectory = operands.size() > 1 && !parser.isPresent(OPT_MERGE) &&
            !parser.isPresent(OPT_MSEMAP) && !parser.isPresent(OPT_RMSEMAP);

    Path output;
    if (parser.isPresent(OPT_OUTPUT)) {
        output = Path(parser.param(OPT_OUTPUT));
        if (isOutputDirectory)
            if (!FileUtils::createDirectory(output, true))
                parser.fail("Unable to create output directory '%s'", parser.param(OPT_OUTPUT));
    }

    float exposure = 0.0f;
    if (parser.isPresent(OPT_EXPOSURE))
        parseFloat(exposure, parser.param(OPT_EXPOSURE));
    Tonemap::Type tonemap("gamma");
    if (parser.isPresent(OPT_TONEMAP)) {
        try {
            tonemap = parser.param(OPT_TONEMAP);
        } catch(const std::runtime_error &) {
            parser.fail("Invalid tonemapping operator: %s", parser.param(OPT_TONEMAP));
        }
    }
    std::string filetype;
    if (parser.isPresent(OPT_FILETYPE)) {
        filetype = parser.param(OPT_FILETYPE);
        if (filetype != "png"
#if OPENEXR_AVAILABLE
            && filetype != "exr"
#endif
            && filetype != "pfm")
                parser.fail("Unsupported output filetype '%s'", filetype);
    }

    bool hdrConvert = !parser.operands().empty();
    for (const std::string &operand : parser.operands())
        hdrConvert = hdrConvert && Path(operand).testExtension("png");

    if (hdrConvert) {
        bool gammaCorrect = !parser.isPresent(OPT_TONEMAP) || parser.param(OPT_TONEMAP) != "linear";
        for (const std::string &operand : parser.operands()) {
            int w, h;
            std::unique_ptr<uint8[]> img = ImageIO::loadLdr(Path(operand), TexelConversion::REQUEST_RGB, w, h, gammaCorrect);

            if (!img) {
                std::cout << tfm::format("Unable to load input file at '%s'", operand) << std::endl;
                continue;
            }

            std::unique_ptr<float[]> hdr(new float[w*h*3]);
            for (int i = 0; i < w*h; ++i) {
                hdr[i*3 + 0] = img[i*4 + 0]*(1.0f/256.0f);
                hdr[i*3 + 1] = img[i*4 + 1]*(1.0f/256.0f);
                hdr[i*3 + 2] = img[i*4 + 2]*(1.0f/256.0f);
            }

            Path out = Path(operand).setExtension("exr");
            ImageIO::saveHdr(out, hdr.get(), w, h, 3);
        }

        return 0;
    }

    if (parser.isPresent(OPT_MERGE)) {
        std::vector<double> weights;
        if (parser.isPresent(OPT_WEIGHTS)) {
            std::stringstream ss(parser.param(OPT_WEIGHTS));
            std::string substr;
            while(ss.good()) {
                std::getline(ss, substr, ',');
                weights.push_back(std::atof(substr.c_str()));
            }
            if (weights.size() != operands.size())
                parser.fail("Number of weights does not match number of input images");
        } else {
            weights.resize(operands.size(), 1.0);
        }

        if (!parser.isPresent(OPT_OUTPUT))
            parser.fail("Missing output file. You need to specify -o when using --merge");
        mergeImages(parser, output, operands, weights, exposure, tonemap);
    } else if (parser.isPresent(OPT_MSE)  || parser.isPresent(OPT_MSEMAP)
            || parser.isPresent(OPT_RMSE) || parser.isPresent(OPT_RMSEMAP)) {
        if (parser.operands().size() != 2)
            parser.fail("Need exactly two input images to compute difference metric");

        int imgWA, imgHA, imgWB, imgHB;
        std::unique_ptr<float[]> imgA = ImageIO::loadHdr(Path(parser.operands()[0]), TexelConversion::REQUEST_RGB, imgWA, imgHA);
        std::unique_ptr<float[]> imgB = ImageIO::loadHdr(Path(parser.operands()[1]), TexelConversion::REQUEST_RGB, imgWB, imgHB);

        if (!imgA) parser.fail("Unable to load input file at '%s'", parser.operands()[0]);
        if (!imgB) parser.fail("Unable to load input file at '%s'", parser.operands()[1]);
        if (imgWA != imgWB || imgHA != imgHB)
            parser.fail("Input images must be of equal size to compute difference metric! "
                        "(have %dx%d and %dx%d)", imgWA, imgHA, imgWB, imgHB);

        std::unique_ptr<float[]> errorMetric;
        if (parser.isPresent(OPT_RMSE)  || parser.isPresent(OPT_RMSEMAP))
            errorMetric = rmseMap(imgWA, imgHA, std::move(imgA), std::move(imgB));
        else
            errorMetric = mseMap(imgWA, imgHA, std::move(imgA), std::move(imgB));

        if (parser.isPresent(OPT_MSEMAP) || parser.isPresent(OPT_RMSEMAP)) {
            if (!parser.isPresent(OPT_OUTPUT))
                parser.fail("Cannot compute difference heatmap: Missing output file");

            for (int i = 0; i < imgWA*imgHA; ++i)
                errorMetric[i] *= 50.0f;

            int percentile = 100;//parser.isPresent(OPT_SSIMMAP) ? 100 : 95;
            std::unique_ptr<float[]> map = heatMap(errorMetric.get(), imgWA, imgHA, percentile);

            outputImage(parser, Path(parser.param(OPT_OUTPUT)), std::move(map), imgWA, imgHA, 0.0f, Tonemap::Type("linear"));
        } else {
            double sum = 0.0;
            for (int i = 0; i < imgWA*imgHA; ++i)
                sum += double(errorMetric[i]);
            double avgError = sum/(imgWA*imgHA);

            std::cout << avgError << std::endl;
        }
    } else if (parser.isPresent(OPT_VARIANCE)) {
        int imgW, imgH;
        ImageIO::loadHdr(Path(operands[0]), TexelConversion::REQUEST_RGB, imgW, imgH);

        auto runningMean = zeroAlloc<float>(imgW*imgH*3);
        auto runningVariance = zeroAlloc<float>(imgW*imgH*3);

        for (size_t i = 0; i < operands.size(); ++i) {
            Path file(operands[i]);

            int imgW, imgH;
            std::unique_ptr<float[]> img = ImageIO::loadHdr(file, TexelConversion::REQUEST_RGB, imgW, imgH);

            for (uint32 j = 0; j < uint32(imgW*imgH*3); ++j) {
                float delta = img[j] - runningMean[j];
                runningMean[j] += delta/(i + 1);
                float delta2 = img[j] - runningMean[j];
                runningVariance[j] += delta*delta2;
            }
        }

        Vec3d result = Vec3d(0.0);
        for (uint32 j = 0; j < uint32(imgW*imgH*3); ++j)
            result[j % 3] += runningVariance[j]/(operands.size() - 1);
        result /= double(imgW*imgH);
        std::cout << result << std::endl;
    } else {
        for (size_t i = 0; i < operands.size(); ++i) {
            Path file(operands[i]);

            int imgW, imgH;
            std::unique_ptr<float[]> img = ImageIO::loadHdr(file, TexelConversion::REQUEST_RGB, imgW, imgH);

            if (!img) {
                std::cout << tfm::format("Unable to load input file at '%s'", operands[i]) << std::endl;
                continue;
            }

            if (parser.isPresent(OPT_AVG)) {
                Vec3d avg(0.0f);
                for (int i = 0; i < imgH*imgW; ++i)
                    avg += Vec3d(Vec3f(img[i*3], img[i*3 + 1], img[i*3 + 2]));
                std::cout << avg/double(imgH*imgW) << std::endl;
                continue;
            }

            Path dstFile = file;
            if (parser.isPresent(OPT_FILETYPE))
                dstFile = dstFile.setExtension(Path(filetype));
            if (operands.size() > 1) {
                if (parser.isPresent(OPT_OUTPUT))
                    dstFile = output/dstFile;
            } else if (parser.isPresent(OPT_OUTPUT)) {
                dstFile = output;
            }

            outputImage(parser, dstFile, std::move(img), imgW, imgH, exposure, tonemap);
        }
    }

    return 0;
}
