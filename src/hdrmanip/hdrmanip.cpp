#include "Version.hpp"

#include "cameras/Tonemap.hpp"

#include "io/FileUtils.hpp"
#include "io/CliParser.hpp"
#include "io/ImageIO.hpp"
#include "io/Path.hpp"

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
        float exposure, Tonemap::Type tonemap)
{
    int resultW = 0, resultH = 0;
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
                    result[(x + y*resultW)*3 + c] += operand[(x + y*w)*3 + c];
    }

    std::unique_ptr<float[]> img(new float[resultW*resultH*3]);
    for (int i = 0; i < resultW*resultH*3; ++i)
        img[i] = float(result[i]/operands.size());

    outputImage(parser, path, std::move(img), resultW, resultH, exposure, tonemap);
}

int main(int argc, const char *argv[])
{
    CliParser parser("hdrmanip", "[options] file1 [file2 [file3 ....]]");
    parser.addOption('h', "help", "Prints this help text", false, OPT_HELP);
    parser.addOption('v', "version", "Prints version information", false, OPT_VERSION);
    parser.addOption('o', "output", "Specifies the output file or directory", true, OPT_OUTPUT);
    parser.addOption('m', "merge", "Merges input files into one by averaging them", false, OPT_MERGE);
    parser.addOption('e', "exposure", "Specifies the exposure to apply to the input image "
            "(default: 0)", true, OPT_EXPOSURE);
    parser.addOption('t', "tonemap", "Specifies the tonemapping operator to apply when "
            "converting to low dynamic range. Available options: linear, gamma, reinhard, filmic. "
            " (default: gamma)", true, OPT_TONEMAP);
    parser.addOption('f', "filetype", "When converting multiple images, specifies the file type to "
            "save the results as. Available options: "
#if OPENEXR_AVAILABLE
            "exr, "
#endif
            "png, pfm.", true, OPT_FILETYPE);

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

    const std::vector<std::string> &operands = parser.operands();

    bool isOutputDirectory = operands.size() > 1 && !parser.isPresent(OPT_MERGE);

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
    Tonemap::Type tonemap = Tonemap::GammaOnly;
    if (parser.isPresent(OPT_TONEMAP)) {
        try {
            tonemap = Tonemap::stringToType(parser.param(OPT_TONEMAP));
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

    if (parser.isPresent(OPT_MERGE)) {
        if (!parser.isPresent(OPT_OUTPUT))
            parser.fail("Missing output file. You need to specify -o when using --merge");
        mergeImages(parser, output, operands, exposure, tonemap);
    } else {
        for (size_t i = 0; i < operands.size(); ++i) {
            Path file(operands[i]);

            int imgW, imgH;
            std::unique_ptr<float[]> img = ImageIO::loadHdr(file, TexelConversion::REQUEST_RGB, imgW, imgH);

            if (!img)
                parser.fail("Unable to load input file at '%s'", operands[i]);

            Path dstFile = file;
            if (operands.size() > 1) {
                if (parser.isPresent(OPT_FILETYPE))
                    dstFile = dstFile.setExtension(Path(filetype));
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
