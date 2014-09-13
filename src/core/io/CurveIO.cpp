#include "CurveIO.hpp"
#include "FileUtils.hpp"

#include <fstream>

namespace Tungsten {

namespace CurveIO {

bool loadHair(const std::string &path, CurveData &data)
{
    std::ifstream in(path, std::ios_base::in | std::ios_base::binary);
    if (!in.good())
        return false;

    char magic[5];
    in.get(magic, 5);
    if (std::string(magic) != "HAIR")
        return false;

    uint32 curveCount, nodeCount, descriptor;
    FileUtils::streamRead(in, curveCount);
    FileUtils::streamRead(in, nodeCount);
    FileUtils::streamRead(in, descriptor);

    bool hasSegments     = (descriptor & 0x01) != 0;
    bool hasPoints       = (descriptor & 0x02) != 0;
    bool hasThickness    = (descriptor & 0x04) != 0;
    bool hasTransparency = (descriptor & 0x08) != 0;
    bool hasColor        = (descriptor & 0x10) != 0;

    // Points are a mandatory field
    if (!hasPoints)
        return false;

    uint32 defaultSegments;
    float defaultThickness;
    float defaultTransparency;
    Vec3f defaultColor;
    FileUtils::streamRead(in, defaultSegments);
    FileUtils::streamRead(in, defaultThickness);
    FileUtils::streamRead(in, defaultTransparency);
    FileUtils::streamRead(in, defaultColor);

    char fileInfo[89];
    in.read(fileInfo, 88);
    fileInfo[88] = '\0';

    if (data.curveEnds) {
        std::vector<uint32> &curveEnds = *data.curveEnds;
        curveEnds.resize(curveCount);
        if (hasSegments) {
            std::vector<uint16> segmentLength(curveCount);
            FileUtils::streamRead(in, segmentLength);
            for (size_t i = 0; i < curveCount; ++i)
                curveEnds[i] = uint32(segmentLength[i]) + 1 + (i > 0 ? curveEnds[i - 1] : 0);
        } else {
            for (size_t i = 0; i < curveCount; ++i)
                curveEnds[i] = (i + 1)*(defaultSegments + 1);
        }
        curveEnds.shrink_to_fit();
    }

    if (data.nodeData) {
        std::vector<Vec4f> &nodeData = *data.nodeData;

        std::vector<Vec3f> points(nodeCount);
        FileUtils::streamRead(in, points);
        nodeData.resize(nodeCount);
        for (size_t i = 0; i < nodeCount; ++i)
            nodeData[i] = Vec4f(points[i].x(), points[i].y(), points[i].z(), defaultThickness);

        if (hasThickness) {
            std::vector<float> thicknesses(nodeCount);
            FileUtils::streamRead(in, thicknesses);
            for (size_t i = 0; i < nodeCount; ++i)
                nodeData[i].w() = thicknesses[i];
        }
        nodeData.shrink_to_fit();
    }

    if (hasTransparency)
        in.seekg(sizeof(float)*nodeCount, std::ios_base::cur);

    if (data.nodeColor) {
        if (hasColor) {
            data.nodeColor->resize(nodeCount);
            FileUtils::streamRead(in, *data.nodeColor);
            data.nodeColor->shrink_to_fit();
        } else {
            data.nodeColor->clear();
            data.nodeColor->push_back(defaultColor);
        }
    }

    return true;
}

bool saveHair(const std::string &path, const CurveData &data)
{
    if (!data.nodeData || !data.curveEnds)
        return false;

    std::ofstream out(path, std::ios_base::out | std::ios_base::binary);
    if (!out.good())
        return false;

    char fileInfo[88] = "Hair file written by Tungsten";
    uint32 descriptor = 0x1 | 0x2 | 0x4; // Segments, points and thickness array
    bool hasColor = data.nodeColor && data.nodeColor->size() == data.nodeData->size();
    if (hasColor)
        descriptor |= 0x10;

    out.put('H');
    out.put('A');
    out.put('I');
    out.put('R');
    FileUtils::streamWrite(out, uint32(data.curveEnds->size()));
    FileUtils::streamWrite(out, uint32(data.nodeData->size()));
    FileUtils::streamWrite(out, descriptor);
    FileUtils::streamWrite(out, uint32(0));
    FileUtils::streamWrite(out, 0.0f);
    FileUtils::streamWrite(out, 0.0f);
    FileUtils::streamWrite(out, Vec3f(1.0f));
    out.write(fileInfo, 88);

    const std::vector<uint32> &curveEnds = *data.curveEnds;
    for (size_t i = 0; i < curveEnds.size(); ++i) {
        uint32 nodeCount = curveEnds[i] - (i ? curveEnds[i - 1] : 0);
        FileUtils::streamWrite(out, nodeCount - 1);
    }
    for (const Vec4f &v : *data.nodeData)
        FileUtils::streamWrite(out, v.xyz());
    for (const Vec4f &v : *data.nodeData)
        FileUtils::streamWrite(out, v.w());
    if (hasColor)
        FileUtils::streamWrite(out, *data.nodeColor);

    return true;
}

bool load(const std::string &path, CurveData &data)
{
    if (FileUtils::testExtension(path, "hair"))
        return loadHair(path, data);
    return false;
}

bool save(const std::string &path, const CurveData &data)
{
    if (FileUtils::testExtension(path, "hair"))
        return saveHair(path, data);
    return false;
}

}

}
