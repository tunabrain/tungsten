#include "Version.hpp"

#include "io/JsonLoadException.hpp"
#include "io/FileIterables.hpp"
#include "io/ZipWriter.hpp"
#include "io/CliParser.hpp"
#include "io/Scene.hpp"
#include "io/Path.hpp"

#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/document.h>
#include <unordered_map>
#include <unordered_set>
#include <iostream>

using namespace Tungsten;

static const int OPT_OUTPUT            = 1;
static const int OPT_VERSION           = 2;
static const int OPT_HELP              = 3;
static const int OPT_RESOURCES         = 4;
static const int OPT_ZIP               = 5;
static const int OPT_COMPRESSION_LEVEL = 6;
static const int OPT_RELOCATE          = 7;
static const int OPT_COPY_RELOCATE     = 8;
static const int OPT_PATHS_ONLY        = 9;

static void listResources(Scene *scene, CliParser &/*parser*/)
{
    for (const auto &p : scene->resources())
        std::cout << *p.second << std::endl;
}

static void zipResources(Scene *scene, CliParser &parser)
{
    if (!parser.isPresent(OPT_OUTPUT))
        parser.fail("No output file specified");

    int compressionLevel = 5;
    if (parser.isPresent(OPT_COMPRESSION_LEVEL))
        compressionLevel = std::atoi(parser.param(OPT_COMPRESSION_LEVEL).c_str());

    std::unordered_set<Path> remappedPaths;
    auto remap = [&](const Path &p) {
        Path result = p.stripParent();
        int index = 1;
        if (p.isDirectory())
            while (remappedPaths.count(result))
                result = p.baseName() + tfm::format("%03d", index++);
        else
            while (remappedPaths.count(result))
                result = p.baseName() + tfm::format("%03d", index++) + p.extension();
        remappedPaths.insert(result);
        return std::move(result);
    };

    try {
        ZipWriter writer(Path(parser.param(OPT_OUTPUT)));
        auto addFile = [&](const Path &src, const Path &dst) {
            if (!writer.addFile(src, dst, compressionLevel))
                std::cout << "Warning: Failed to add file " << src << " to zip package" << std::endl;
        };
        auto addDirectory = [&](const Path &dst) {
            if (!writer.addDirectory(dst))
                std::cout << "Warning: Failed to add directory " << dst << " to zip package" << std::endl;
        };

        for (const auto &r : scene->resources()) {
            Path &path = *r.second;
            Path zipPath = remap(path);

            if (path.isDirectory()) {
                Path root(path, "");
                for (const Path &p : root.recursive()) {
                    if (p.isDirectory())
                        addDirectory(zipPath/p);
                    else
                        addFile(p, zipPath/p);
                }
            } else {
                addFile(path, zipPath);
            }

            path = zipPath;
        }

        rapidjson::Document document;
        document.SetObject();
        *(static_cast<rapidjson::Value *>(&document)) = scene->toJson(document.GetAllocator());

        rapidjson::GenericStringBuffer<rapidjson::UTF8<>> buffer;
        rapidjson::PrettyWriter<rapidjson::GenericStringBuffer<rapidjson::UTF8<>>> jsonWriter(buffer);
        document.Accept(jsonWriter);

        std::string json = buffer.GetString();
        writer.addFile(json.c_str(), json.size(), scene->path().fileName(), compressionLevel);
    } catch (const std::runtime_error &e) {
        parser.fail("Failed to package zip: %s", e.what());
    }
}

static void relocateResources(Scene *scene, CliParser &parser)
{
    if (!parser.isPresent(OPT_OUTPUT))
        parser.fail("No output file specified");

    Path output(parser.param(OPT_OUTPUT));
    if (!FileUtils::createDirectory(output, true))
        parser.fail("Failed to create output directory at '%s'", output);

    Path resourceParent = output;

    Path normalizedOutput = output.normalize();
    Path outputTail;
    Path normalizedSceneFolder = scene->path().parent().normalize();
    while (!normalizedOutput.empty()) {
        if (normalizedOutput == normalizedSceneFolder) {
            resourceParent = outputTail;
            break;
        }
        outputTail = normalizedOutput.fileName()/outputTail;
        normalizedOutput = normalizedOutput.parent().stripSeparator();
    }

    for (const auto &r : scene->resources()) {
        Path newPath = output/r.first.fileName();

        bool success;
        if (parser.isPresent(OPT_PATHS_ONLY))
            success = true;
        else if (parser.isPresent(OPT_COPY_RELOCATE))
            success = FileUtils::copyFile(r.first, newPath, false);
        else
            success = FileUtils::moveFile(r.first, newPath, true);

        if (!success)
            std::cout << "Failed to relocate resource " << r.first << std::endl;
        else
            *r.second = resourceParent/r.first.fileName();
    }

    Scene::save(scene->path(), *scene);
}

int main(int argc, const char *argv[])
{
    CliParser parser("scenemanip");
    parser.addOption('h', "help", "Prints this help text", false, OPT_HELP);
    parser.addOption('v', "version", "Prints version information", false, OPT_VERSION);
    parser.addOption('r', "resources", "Lists all resources referenced by the scene file", false, OPT_RESOURCES);
    parser.addOption('z', "zip", "Packs all referenced resources as well as the scene file into a zip file", false, OPT_ZIP);
    parser.addOption('o', "output", "Specifies the output file or directory", true, OPT_OUTPUT);
    parser.addOption('\0', "compression-level", "Specifies the compression level for zip packaging", true, OPT_COMPRESSION_LEVEL);
    parser.addOption('\0', "relocate", "Moves all resources referenced by the scene file into the specified output directory", false, OPT_RELOCATE);
    parser.addOption('\0', "copy", "Copy resources instead of moving them when running --relocate", false, OPT_COPY_RELOCATE);
    parser.addOption('\0', "paths-only", "Only modify resource paths in the scene file when running --relocate, don't copy or move any files", false, OPT_PATHS_ONLY);

    parser.parse(argc, argv);

    if (parser.isPresent(OPT_VERSION)) {
        std::cout << "scenemanip, version " << VERSION_STRING << std::endl;
        return 0;
    }
    if (argc < 2 || parser.isPresent(OPT_HELP)) {
        parser.printHelpText();
        return 0;
    }
    if (parser.operands().empty())
        parser.fail("No input files");
    if (parser.operands().size() > 1)
        parser.fail("Too many input files");

    Scene *scene;
    try {
        scene = Scene::load(Path(parser.operands()[0]));
    } catch (const JsonLoadException &e) {
        std::cerr << e.what() << std::endl;
    }

    if (parser.isPresent(OPT_RESOURCES))
        listResources(scene, parser);
    else if (parser.isPresent(OPT_ZIP))
        zipResources(scene, parser);
    else if (parser.isPresent(OPT_RELOCATE))
        relocateResources(scene, parser);
    else
        parser.fail("Don't know what to do! No action specified");

    return 0;
}
