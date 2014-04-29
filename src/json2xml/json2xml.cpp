#include "JsonXmlConverter.hpp"

#include "io/FileUtils.hpp"
#include "io/Scene.hpp"

#include <embree/include/embree.h>
#include <iostream>

using namespace Tungsten;

int main(int argc, const char *argv[])
{
    if (argc != 3) {
        std::cerr << "Usage: json2xml inputfile outputfile\n";
        return 1;
    }

    std::string dst(argv[2]);
    std::string dstDir(FileUtils::extractDir(dst));
    if (!dstDir.empty() && !FileUtils::createDirectory(dstDir)) {
        std::cerr << "Unable to create target directory '" << dstDir <<"'\n";
        return 1;
    }

    Scene *scene;
    try {
        scene = Scene::load(argv[1]);
    } catch (std::runtime_error &e) {
        std::cerr << "Scene loader encountered an unrecoverable error: \n" << e.what() << std::endl;
        return 1;
    }

    if (!scene) {
        std::cerr << "Unable to open input file '" << argv[1] << "'\n";
        return 1;
    }

    std::ofstream out(dst);
    if (!out.good()) {
        std::cerr << "Unable to write to target file '" << argv[2] << "'\n";
        return 1;
    }

    embree::rtcInit();
    embree::rtcStartThreads(8);

    try {
        SceneXmlWriter writer(dstDir, *scene, out);
    } catch (std::runtime_error &e) {
        std::cerr << "SceneXmlWriter encountered an unrecoverable error: \n" << e.what() << std::endl;
        return 1;
    }

    return 0;
}
