#include <iostream>

#include "io/Scene.hpp"

using namespace Tungsten;

int main(int argc, const char *argv[])
{
    if (argc != 2) {
        std::cerr << "Usage: scenemanip inputfile\n";
        return 1;
    }

    Scene *scene;
    try {
        scene = Scene::load(Path(argv[1]));
    } catch (std::runtime_error &e) {
        std::cerr << "Scene loader encountered an unrecoverable error: \n" << e.what() << std::endl;
        return false;
    }

    for (const auto &p : scene->resources())
        std::cout << *p.second << std::endl;

    return 0;
}
