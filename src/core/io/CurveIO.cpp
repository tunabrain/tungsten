#include "CurveIO.hpp"
#include "FileUtils.hpp"

#include "sampling/UniformSampler.hpp"
#include "sampling/SampleWarp.hpp"

#include "math/BSpline.hpp"
#include "math/Angle.hpp"
#include "math/Mat4f.hpp"

#include <fstream>

namespace Tungsten {

namespace CurveIO {

void extrudeMinimumTorsionNormals(CurveData &data)
{
    std::vector<uint32> &curveEnds = *data.curveEnds;
    std::vector<Vec4f> &nodes = *data.nodeData;
    std::vector<Vec3f> &normals = *data.nodeNormal;

    auto getTangent = [&](int idx, float t)
    {
        const Vec4f &p0 = nodes[idx];
        const Vec4f &p1 = nodes[idx + 1];
        const Vec4f &p2 = nodes[idx + 2];
        return BSpline::quadraticDeriv(p0.xyz(), p1.xyz(), p2.xyz(), t).normalized();
    };

    auto minTorsionAdvance = [&](const Vec3f &currentNormal, int idx) {
        Vec3f tangent = getTangent(idx, 0.0f);
        Vec3f nextNormal = currentNormal;
        for (int j = 1; j <= 10; ++j) {
            Vec3f newTangent = getTangent(idx, j*0.1f);
            float angle = std::acos(std::min(tangent.dot(newTangent), 1.0f));
            Vec3f axis = tangent.cross(newTangent);
            float length = axis.length();
            if (length == 0.0f)
                continue;
            axis /= length;
            tangent = newTangent;

            nextNormal = Mat4f::rotAxis(axis, Angle::radToDeg(angle))*nextNormal;
            nextNormal -= tangent*tangent.dot(nextNormal);
            nextNormal.normalize();
        }
        return nextNormal;
    };

    uint32 t = 0;
    for (size_t i = 0; i < curveEnds.size(); ++i) {
        Vec3f lastNormal = normals[t];
        do {
            normals[t + 1] = (2.0f*lastNormal - normals[t]).normalized();
            lastNormal = minTorsionAdvance(lastNormal, t);
        } while (++t < curveEnds[i] - 2);
        normals[t + 1] = normals[t];
        t += 2;
    }
}

void initializeRandomNormals(CurveData &data)
{
    std::vector<uint32> &curveEnds = *data.curveEnds;
    std::vector<Vec4f> &nodes = *data.nodeData;
    std::vector<Vec3f> &normals = *data.nodeNormal;
    normals.resize(nodes.size());

    UniformSampler sampler(std::hash<Vec3f>()(data.nodeData->front().xyz()));
    for (size_t i = 0; i < curveEnds.size(); ++i) {
        uint32 dst = (i == 0 ? 0 : curveEnds[i - 1]);

        Vec3f tangent((nodes[dst + 1].xyz() - nodes[dst].xyz()).normalized());
        float dot;
        do {
            normals[dst] = SampleWarp::uniformSphere(sampler.next2D());
            dot = tangent.dot(normals[dst]);
        } while (std::abs(dot) > 1.0f - 1e-4f);
        normals[dst] -= tangent*dot;
        normals[dst].normalize();
    }

    extrudeMinimumTorsionNormals(data);
}

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

    if (data.curveEnds && data.nodeData && data.nodeNormal)
        initializeRandomNormals(data);

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
