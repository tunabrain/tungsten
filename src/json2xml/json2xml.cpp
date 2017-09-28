#include "JsonXmlConverter.hpp"
#include "Version.hpp"

#include "primitives/EmbreeUtil.hpp"

#include "io/JsonLoadException.hpp"
#include "io/CliParser.hpp"
#include "io/FileUtils.hpp"
#include "io/Scene.hpp"

#include <iostream>

using namespace Tungsten;

void convert(CliParser &parser, const Path &src, const Path &dst)
{
    Path dstDir = dst.parent();
    if (!dstDir.empty() && !FileUtils::createDirectory(dstDir))
        parser.fail("Unable to create target directory '%s'", dstDir);

    Scene *scene;
    try {
        scene = Scene::load(src);
        scene->loadResources();
    } catch (const JsonLoadException &e) {
        std::cerr << e.what() << std::endl;
    }

    if (!scene)
        parser.fail("Unable to open input file '%s'", src);

    OutputStreamHandle out = FileUtils::openOutputStream(dst);
    if (!out)
        parser.fail("Unable to write to target file '%s'", dst);

    try {
        SceneXmlWriter writer(dstDir, *scene, *out);
    } catch (std::runtime_error &e) {
        parser.fail("SceneXmlWriter encountered an unrecoverable error: %s", e.what());
    }
}

static const int OPT_VERSION = 0;
static const int OPT_HELP    = 1;

int main(int argc, const char *argv[])
{
    CliParser parser("json2xml", "[options] inputfile outputfile");
    parser.addOption('h', "help", "Prints this help text", false, OPT_HELP);
    parser.addOption('v', "version", "Prints version information", false, OPT_VERSION);

    parser.parse(argc, argv);

    if (parser.isPresent(OPT_VERSION)) {
        std::cout << "json2xml, version " << VERSION_STRING << std::endl;
        return 0;
    }

    if (parser.operands().size() != 2 || parser.isPresent(OPT_HELP)) {
        parser.printHelpText();
        return 0;
    }
    EmbreeUtil::initDevice();

    convert(parser, Path(parser.operands()[0]), Path(parser.operands()[1]));

    return 0;
}
