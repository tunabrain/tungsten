#ifndef RESOURCEPACKLOADER_HPP_
#define RESOURCEPACKLOADER_HPP_

#include "BlockDescriptor.hpp"
#include "ModelResolver.hpp"
#include "Model.hpp"
#include "File.hpp"

#include "io/FileUtils.hpp"
#include "io/JsonUtils.hpp"

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>

namespace Tungsten {

static inline std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim))
        elems.push_back(item);
    return std::move(elems);
}

class ResourcePackLoader {
    std::string _packPath;

    std::vector<BlockDescriptor> _blockDescriptors;
    std::vector<Model> _models;
    std::vector<const BlockVariant *> _blockMapping;

    std::unique_ptr<ModelResolver> _resolver;

    void buildBlockMapping()
    {
        _blockMapping.resize(65536, nullptr);

        std::string json;
        try {
            json = FileUtils::loadText(_packPath + blockMapPath);
        } catch(std::runtime_error &e) {
            return;
        }
        rapidjson::Document document;
        document.Parse<0>(json.c_str());
        if (document.HasParseError() || !document.IsArray())
            return;

        std::unordered_map<std::string, const BlockVariant *> blockMap;
        for (const BlockDescriptor &b : _blockDescriptors) {
            for (const BlockVariant &v : b.variants())
                blockMap.insert(std::make_pair(b.name() + "." + v.variant(), &v));
            blockMap.insert(std::make_pair(b.name(), &b.variants().front()));
        }

        for (unsigned i = 0; i < document.Size(); ++i) {
            int id   = JsonUtils::as<int>(document[i], "id");
            int data = JsonUtils::as<int>(document[i], "data");
            std::string blockState = JsonUtils::as<std::string>(document[i], "blockstate");
            std::string variant = "normal";
            JsonUtils::fromJson(document[i], "variant", variant);
            std::string key = blockState + "." + variant;

            int index = (id << 4) | data;

            if (!blockMap.count(key)) {
                std::cout << "Warning: Could not find block " << blockState << " with variant " << variant;
                if (blockMap.count(blockState)) {
                    std::cout << " Using variant " << blockMap[blockState]->variant() << " instead";
                    _blockMapping[index] = blockMap[blockState];
                }
                std::cout << std::endl;
            } else {
                _blockMapping[index] = blockMap[key];
            }
        }
    }

    void loadStates()
    {
        File stateFolder(_packPath + stateBase);
        for (File iter = stateFolder.beginScan(File::extFilter("json")); iter.valid(); iter = stateFolder.scan()) {
            std::string json = FileUtils::loadText(iter.path().c_str());

            if (json.empty())
                continue;

            rapidjson::Document document;
            document.Parse<0>(json.c_str());
            if (document.HasParseError() || !document.IsObject())
                continue;

            _blockDescriptors.emplace_back(iter.baseName(), document, *_resolver);
        }
    }

    void loadModels(File dir, std::string base = "")
    {
        base = FileUtils::addSeparator(base);

        for (File iter = dir.beginScan(File::dirFilter()); iter.valid(); iter = dir.scan())
            loadModels(iter, base + iter.name());

        for (File iter = dir.beginScan(File::extFilter("json")); iter.valid(); iter = dir.scan()) {
            std::string json = FileUtils::loadText(iter.path().c_str());

            if (json.empty())
                continue;

            rapidjson::Document document;
            document.Parse<0>(json.c_str());
            if (document.HasParseError() || !document.IsObject())
                continue;

            _models.emplace_back(base + iter.baseName(), document);
        }
    }

    void printVariants()
    {
        std::map<std::string, std::pair<std::set<std::string>, std::set<std::string>>> variants;

        for (const BlockDescriptor &desc : _blockDescriptors) {
            for (const BlockVariant &variant : desc.variants()) {
                for (std::string subVariant : split(variant.variant(), ',')) {
                    auto keyValue = split(subVariant, '=');
                    variants[keyValue[0]].first.insert(desc.name());
                    if (keyValue.size() == 2)
                        variants[keyValue[0]].second.insert(keyValue[1]);
                }
            }
        }

        for (const auto &variant : variants) {
            std::cout << variant.first << " {";
            size_t i = 0;
            for (const std::string &client : variant.second.first)
                std::cout << client << (++i != variant.second.first.size() ? ", " : "}");
            std::cout << std::endl;

            for (const std::string &value : variant.second.second)
                std::cout << "  " << value << std::endl;
        }
    }

public:
    static const char *modelBase;
    static const char *stateBase;
    static const char *textureBase;
    static const char *blockMapPath;

    ResourcePackLoader(const std::string packPath)
    : _packPath(FileUtils::addSeparator(packPath))
    {
        loadModels(File(_packPath + modelBase));
        _resolver.reset(new ModelResolver(_models));
        loadStates();
        buildBlockMapping();
    }

    const std::vector<BlockDescriptor> &blockDescriptors() const
    {
        return _blockDescriptors;
    }

    std::string textureBasePath() const
    {
        return FileUtils::addSeparator(_packPath) + textureBase;
    }

    const ModelRef *mapBlock(uint16 id, int idx) const
    {
        const BlockVariant *variant = _blockMapping[id];
        if (!variant)
            variant = _blockMapping[id & 0xFFF0];
        if (!variant)
            return nullptr;

        int model = MathUtil::hash32(idx) % variant->models().size();

        return &variant->models()[model];
    }
};

}

#endif /* RESOURCEPACKLOADER_HPP_ */
