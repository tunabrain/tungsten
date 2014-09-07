#include <iostream>

#include "io/ObjLoader.hpp"
#include "io/FileUtils.hpp"
#include "io/Scene.hpp"

using namespace Tungsten;

int main(int argc, const char *argv[])
{
    if (argc != 3) {
        std::cerr << "Usage: obj2json inputfile outputfile\n";
        return 1;
    }

    std::string dst(argv[2]);
    std::string dstDir(FileUtils::extractParent(dst));
    if (!dstDir.empty() && !FileUtils::createDirectory(dstDir)) {
        std::cerr << "Unable to create target directory '" << dstDir <<"'\n";
        return 1;
    }

    Scene *scene = ObjLoader::load(argv[1]);

    if (!scene) {
        std::cerr << "Unable to open input file '" << argv[1] << "'\n";
        return 1;
    }

    Scene::save(dst, *scene, true);

    return 0;
}
