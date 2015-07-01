#include "CurveIO.hpp"
#include "ObjLoader.hpp"
#include "FileUtils.hpp"

#include "sampling/UniformSampler.hpp"
#include "sampling/SampleWarp.hpp"

#include "thread/ThreadUtils.hpp"

#include "math/BSpline.hpp"
#include "math/Angle.hpp"
#include "math/Mat4f.hpp"

#include "io/FileUtils.hpp"

namespace Tungsten {

namespace CurveIO {

void extrudeMinimumTorsionNormals(CurveData &data)
{
    std::vector<uint32> &curveEnds = *data.curveEnds;
    std::vector<Vec4f> &nodes = *data.nodeData;
    std::vector<Vec3f> &normals = *data.nodeNormal;

    if (nodes.empty())
        return;

    auto minTorsionAdvance = [&](const Vec3f &currentNormal, int idx) {
        Vec3f p0 = nodes[idx + 0].xyz();
        Vec3f p1 = nodes[idx + 1].xyz();
        Vec3f p2 = nodes[idx + 2].xyz();
        Vec3f deriv0(p1 - p0), deriv1(p0 - 2.0f*p1 + p2);

        Vec3f T0 = deriv0.normalized();
        Vec3f N = currentNormal;
        for (int j = 1; j <= 5; ++j) {
            Vec3f T1 = (j*0.2f*deriv1 + deriv0).normalized();
            Vec3f A = T0.cross(T1);
            float length = A.length();
            if (length == 0.0f)
                continue;
            A *= 1.0f/length;

            Vec3f A0 = T0.cross(A);
            Vec3f A1 = T1.cross(A);

            N = Vec3f(
                N.x()*T1.x() + N.y()*A.x() + N.z()*A1.x(),
                N.x()*T1.y() + N.y()*A.y() + N.z()*A1.y(),
                N.x()*T1.z() + N.y()*A.z() + N.z()*A1.z()
            );
            N = Vec3f(T0.dot(N), A.dot(N), A0.dot(N));
            T0 = T1;
        }
        N -= T0*T0.dot(N);
        N.normalize();
        return N;
    };

    // Break work up into roughly ~10ms chunks
    uint32 numTasks = (int(nodes.size()) - 1)/30000 + 1;
    ThreadUtils::parallelFor(0, uint32(curveEnds.size()), numTasks, [&](uint32 i) {
        uint32 t = i ? curveEnds[i - 1] : 0;
        Vec3f lastNormal = normals[t];
        do {
            normals[t + 1] = (2.0f*lastNormal - normals[t]).normalized();
            lastNormal = minTorsionAdvance(lastNormal, t);
        } while (++t < curveEnds[i] - 2);
        normals[t + 1] = normals[t];
    });
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

bool loadObj(const Path &path, CurveData &data)
{
    if (!data.curveEnds || !data.nodeData)
        return false;

    if (!ObjLoader::loadCurvesOnly(path, *data.curveEnds, *data.nodeData))
        return false;

    if (data.nodeColor) {
        data.nodeColor->clear();
        data.nodeColor->push_back(Vec3f(1.0f));
    }
    if (data.nodeNormal)
        initializeRandomNormals(data);

    return true;
}

bool loadHair(const Path &path, CurveData &data)
{
    InputStreamHandle in = FileUtils::openInputStream(path);
    if (!in)
        return false;

    char magic[5];
    in->get(magic, 5);
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
    in->read(fileInfo, 88);
    fileInfo[88] = '\0';

    if (data.curveEnds) {
        std::vector<uint32> &curveEnds = *data.curveEnds;
        curveEnds.resize(curveCount);
        if (hasSegments) {
            std::vector<uint16> segmentLength(curveCount);
            FileUtils::streamRead(in, segmentLength);
            for (uint32 i = 0; i < curveCount; ++i)
                curveEnds[i] = uint32(segmentLength[i]) + 1 + (i > 0 ? curveEnds[i - 1] : 0);
        } else {
            for (uint32 i = 0; i < curveCount; ++i)
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
        in->seekg(sizeof(float)*nodeCount, std::ios_base::cur);

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

bool saveHair(const Path &path, const CurveData &data)
{
    if (!data.nodeData || !data.curveEnds)
        return false;

    OutputStreamHandle out = FileUtils::openOutputStream(path);
    if (!out)
        return false;

    char fileInfo[88] = "Hair file written by Tungsten";
    uint32 descriptor = 0x1 | 0x2 | 0x4; // Segments, points and thickness array
    bool hasColor = data.nodeColor && data.nodeColor->size() == data.nodeData->size();
    if (hasColor)
        descriptor |= 0x10;

    out->put('H');
    out->put('A');
    out->put('I');
    out->put('R');
    FileUtils::streamWrite(out, uint32(data.curveEnds->size()));
    FileUtils::streamWrite(out, uint32(data.nodeData->size()));
    FileUtils::streamWrite(out, descriptor);
    FileUtils::streamWrite(out, uint32(0));
    FileUtils::streamWrite(out, 0.0f);
    FileUtils::streamWrite(out, 0.0f);
    FileUtils::streamWrite(out, Vec3f(1.0f));
    out->write(fileInfo, 88);

    const std::vector<uint32> &curveEnds = *data.curveEnds;
    for (size_t i = 0; i < curveEnds.size(); ++i) {
        uint32 nodeCount = curveEnds[i] - (i ? curveEnds[i - 1] : 0);
        FileUtils::streamWrite(out, uint16(nodeCount - 1));
    }
    for (const Vec4f &v : *data.nodeData)
        FileUtils::streamWrite(out, v.xyz());
    for (const Vec4f &v : *data.nodeData)
        FileUtils::streamWrite(out, v.w());
    if (hasColor)
        FileUtils::streamWrite(out, *data.nodeColor);

    return true;
}

bool load(const Path &path, CurveData &data)
{
    if (path.testExtension("hair"))
        return loadHair(path, data);
    else if (path.testExtension("obj"))
        return loadObj(path, data);
    return false;
}

bool save(const Path &path, const CurveData &data)
{
    if (path.testExtension("hair"))
        return saveHair(path, data);
    return false;
}

}

}
