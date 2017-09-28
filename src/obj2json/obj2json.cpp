#include <iostream>

#include "Version.hpp"

#include "io/DirectoryChange.hpp"
#include "io/CliParser.hpp"
#include "io/ObjLoader.hpp"
#include "io/FileUtils.hpp"
#include "io/Scene.hpp"

using namespace Tungsten;

static const int OPT_VERSION = 0;
static const int OPT_HELP    = 1;

int main(int argc, const char *argv[])
{
    CliParser parser("obj2json", "[options] inputfile outputfile");
    parser.addOption('h', "help", "Prints this help text", false, OPT_HELP);
    parser.addOption('v', "version", "Prints version information", false, OPT_VERSION);

    parser.parse(argc, argv);

    if (parser.isPresent(OPT_VERSION)) {
        std::cout << "obj2json, version " << VERSION_STRING << std::endl;
        return 0;
    }
    if (parser.operands().size() != 2 || parser.isPresent(OPT_HELP)) {
        parser.printHelpText();
        return 0;
    }

    Path dst(parser.operands()[1]);
    Path dstDir = dst.parent();
    if (!dstDir.empty() && !FileUtils::createDirectory(dstDir))
        parser.fail("Unable to create target directory '%s'", dstDir);

    Scene *scene = ObjLoader::load(Path(parser.operands()[0]));

    if (!scene)
        parser.fail("Unable to open input file '%s'", parser.operands()[0]);

    Scene::save(dst, *scene);

    DirectoryChange change(dstDir);
    scene->saveResources();

    return 0;
}
