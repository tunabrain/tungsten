#include "JsonXmlConverter.hpp"

#include "io/FileUtils.hpp"
#include "io/Scene.hpp"

#include <embree/include/embree.h>
#include <iostream>

using namespace Tungsten;

bool convert(const Path &src, const Path &dst)
{
    Path dstDir = dst.parent();
    if (!dstDir.empty() && !FileUtils::createDirectory(dstDir)) {
        std::cerr << "Unable to create target directory '" << dstDir <<"'\n";
        return false;
    }

    Scene *scene;
    try {
        scene = Scene::load(src);
        scene->loadResources();
    } catch (std::runtime_error &e) {
        std::cerr << "Scene loader encountered an unrecoverable error: \n" << e.what() << std::endl;
        return false;
    }

    if (!scene) {
        std::cerr << "Unable to open input file '" << src << "'\n";
        return false;
    }

    OutputStreamHandle out = FileUtils::openOutputStream(dst);
    if (!out) {
        std::cerr << "Unable to write to target file '" << dst << "'\n";
        return false;
    }

    try {
        SceneXmlWriter writer(dstDir, *scene, *out);
    } catch (std::runtime_error &e) {
        std::cerr << "SceneXmlWriter encountered an unrecoverable error: \n" << e.what() << std::endl;
        return false;
    }
    return true;
}

int main(int argc, const char *argv[])
{
    if (argc < 3) {
        std::cerr << "Usage: json2xml inputfile outputfile\n";
        return 1;
    }

    embree::rtcInit();
    embree::rtcStartThreads(8);

    if (argc == 3) {
        convert(Path(argv[1]), Path(argv[2]));
    } else {
        for (int i = 1; i < argc; ++i)
            convert(Path(argv[i]), Path(argv[i]).setExtension("xml"));
    }

    return 0;
}
