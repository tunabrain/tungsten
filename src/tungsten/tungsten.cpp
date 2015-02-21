#include "Version.hpp"
#include "Shared.hpp"

using namespace Tungsten;

int main(int argc, const char *argv[])
{
    CliParser parser("tungsten", "[options] scene1 [scene2 [scene3...]]");

    StandaloneRenderer renderer(parser, std::cout);

    parser.parse(argc, argv);

    if (parser.isPresent(OPT_VERSION)) {
        std::cout << "tungsten, version " << VERSION_STRING << std::endl;
        std::exit(0);
    }

    renderer.setup();

    while (renderer.renderScene());

    return 0;
}
