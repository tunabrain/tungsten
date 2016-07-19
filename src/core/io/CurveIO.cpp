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

#include "Platform.hpp"

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

bool saveMitsubaHair(const Path &path, const CurveData &data)
{
    if (!data.nodeData || !data.curveEnds)
        return false;
    
    OutputStreamHandle out = FileUtils::openOutputStream(path);
    if (!out)
        return false;
    
    out->write("BINARY_HAIR", 11);
    FileUtils::streamWrite(out, uint32(data.nodeData->size()));
    
    const std::vector<uint32> &curveEnds = *data.curveEnds;
    const std::vector<Vec4f> &nodeData = *data.nodeData;
    size_t curveIdx = 0;
    for (size_t i = 0; i < nodeData.size(); ++i) {
        FileUtils::streamWrite(out, nodeData[i].xyz());
        if (i + 1 == curveEnds[curveIdx]) {
            FileUtils::streamWrite(out, std::numeric_limits<float>::infinity());
            curveIdx++;
        }
    }
    
    return true;
}

namespace Fiber {

static const std::array<uint8, 8> FiberMagic{0x80, 0xBF, 0x80, 0x46, 0x49, 0x42, 0x45, 0x52};
static const uint16 SupportedMajorVersion = 1;
static const uint16 CurrentMinorVersion = 0;
static const uint32 SupportedContentType = 0;

static const size_t FiberValueSize[] = {1, 1, 2, 2, 4, 4, 8, 8, 4, 8};
enum FiberValueType
{
    FIBER_INT8   = 0,
    FIBER_UINT8  = 1,
    FIBER_INT16  = 2,
    FIBER_UINT16 = 3,
    FIBER_INT32  = 4,
    FIBER_UINT32 = 5,
    FIBER_INT64  = 6,
    FIBER_UINT64 = 7,
    FIBER_FLOAT  = 8,
    FIBER_DOUBLE = 9,
    FIBER_TYPE_COUNT
};

struct FiberAttribute
{
    uint64 dataLength;
    uint16 attributeFlags;
    uint8 valueType;
    uint8 valuesPerElement;
    std::string attributeName;
    uint64 elementsPresent;

    FiberAttribute(InputStreamHandle in)
    : dataLength(FileUtils::streamRead<uint64>(in)),
      attributeFlags(FileUtils::streamRead<uint16>(in)),
      valueType(FileUtils::streamRead<uint8>(in)),
      valuesPerElement(FileUtils::streamRead<uint8>(in)),
      attributeName(FileUtils::streamRead<std::string>(in)),
      elementsPresent(0)
    {
        if (valueType < FIBER_TYPE_COUNT)
            elementsPresent = dataLength/(FiberValueSize[valueType]*valuesPerElement);
    }

    bool matches(const std::string &name, bool perCurve, FiberValueType type, int numValuesPerElement)
    {
        return attributeName == name
            && ((attributeFlags & 1) != 0) == perCurve
            && FiberValueType(valueType) == type
            && valuesPerElement == numValuesPerElement
            && elementsPresent > 0;
    }

    template<typename T>
    std::unique_ptr<T[]> load(InputStreamHandle in, uint64 elementsRequired)
    {
        std::unique_ptr<T[]> result(new T[size_t(elementsRequired)]);
        FileUtils::streamRead(in, result.get(), size_t(elementsPresent));
        // Copy-extend
        for (size_t i = size_t(elementsPresent); i < size_t(elementsRequired); ++i)
            result[i] = result[size_t(elementsPresent - 1)];
        return std::move(result);
    }
};
static bool loadFiber(const Path &path, CurveData &data)
{
    InputStreamHandle in = FileUtils::openInputStream(path);
    if (!in)
        return false;

    if (FileUtils::streamRead<std::array<uint8, 8>>(in) != FiberMagic)
        return false;

    uint16 versionMajor = FileUtils::streamRead<uint16>(in);
    uint16 versionMinor = FileUtils::streamRead<uint16>(in);
    MARK_UNUSED(versionMinor);
    if (versionMajor != SupportedMajorVersion)
        return false;

    uint32 contentType = FileUtils::streamRead<uint32>(in);
    if (contentType != SupportedContentType)
        return false;

    uint64 headerLength = FileUtils::streamRead<uint64>(in);
    uint64 numVertices  = FileUtils::streamRead<uint64>(in);
    uint64 numCurves    = FileUtils::streamRead<uint64>(in);

    uint64 offset = headerLength;
    while (true) {
        in->seekg(size_t(offset), std::ios_base::beg);

        uint64 descriptorLength = FileUtils::streamRead<uint64>(in);
        if (descriptorLength == 0)
            break;

        FiberAttribute attribute(in);

        offset += descriptorLength;
        in->seekg(size_t(offset), std::ios_base::beg);

        if (data.curveEnds && attribute.matches("num_vertices", true, FIBER_UINT16, 1)) {
            data.curveEnds->resize(size_t(numCurves));
            auto vertexCounts = attribute.load<uint16>(in, numCurves);
            for (size_t i = 0; i < size_t(numCurves); ++i)
                (*data.curveEnds)[i] = uint32(vertexCounts[i]) + (i > 0 ? (*data.curveEnds)[i - 1] : 0);
        } else if (data.nodeData && attribute.matches("position", false, FIBER_FLOAT, 3)) {
            data.nodeData->resize(size_t(numVertices));
            auto pos = attribute.load<Vec3f>(in, numVertices);
            for (size_t i = 0; i < size_t(numVertices); ++i)
                (*data.nodeData)[i] = Vec4f(pos[i].x(), pos[i].y(), pos[i].z(), (*data.nodeData)[i].w());
        } else if (data.nodeData && attribute.matches("width", false, FIBER_FLOAT, 1)) {
            data.nodeData->resize(size_t(numVertices));
            auto width = attribute.load<float>(in, numVertices);
            for (size_t i = 0; i < size_t(numVertices); ++i)
                (*data.nodeData)[i].w() = width[i];
        }

        offset += attribute.dataLength;
    }

    if (data.curveEnds && data.nodeData && data.nodeNormal)
        initializeRandomNormals(data);

    return true;
}

static void writeFiberAttributeDescriptor(OutputStreamHandle out, const std::string &name,
        uint64 dataLength, bool perCurve, FiberValueType type, int valuesPerElement)
{
    FileUtils::streamWrite(out, uint64(20 + (name.size() + 1)));
    FileUtils::streamWrite(out, dataLength);
    FileUtils::streamWrite(out, uint16(perCurve ? 1 : 0));
    FileUtils::streamWrite(out, uint8(type));
    FileUtils::streamWrite(out, uint8(valuesPerElement));
    FileUtils::streamWrite(out, &name[0], name.size() + 1);
}
static bool saveFiber(const Path &path, const CurveData &data)
{
    if (!data.nodeData || !data.curveEnds)
        return false;

    OutputStreamHandle out = FileUtils::openOutputStream(path);
    if (!out)
        return false;

    const uint64 HeaderLength = 40;
    uint64 numCurves = data.curveEnds->size();
    uint64 numVertices  = data.nodeData->size();

    FileUtils::streamWrite(out, FiberMagic);
    FileUtils::streamWrite(out, SupportedMajorVersion);
    FileUtils::streamWrite(out, CurrentMinorVersion);
    FileUtils::streamWrite(out, SupportedContentType);
    FileUtils::streamWrite(out, HeaderLength);
    FileUtils::streamWrite(out, numVertices);
    FileUtils::streamWrite(out, numCurves);

    writeFiberAttributeDescriptor(out, "num_vertices", numCurves*sizeof(uint16), true, FIBER_UINT16, 1);
    const std::vector<uint32> &curveEnds = *data.curveEnds;
    for (size_t i = 0; i < size_t(numCurves); ++i)
        FileUtils::streamWrite(out, uint16(curveEnds[i] - (i ? curveEnds[i - 1] : 0)));
    writeFiberAttributeDescriptor(out, "position", numVertices*sizeof(Vec3f), false, FIBER_FLOAT, 3);
    for (const Vec4f &v : *data.nodeData)
        FileUtils::streamWrite(out, v.xyz());
    writeFiberAttributeDescriptor(out, "width", numVertices*sizeof(float), false, FIBER_FLOAT, 1);
    for (const Vec4f &v : *data.nodeData)
        FileUtils::streamWrite(out, v.w());

    FileUtils::streamWrite(out, uint64(0));

    return true;
}

}

bool load(const Path &path, CurveData &data)
{
    if (path.testExtension("hair"))
        return loadHair(path, data);
    else if (path.testExtension("fiber"))
        return Fiber::loadFiber(path, data);
    else if (path.testExtension("obj"))
        return loadObj(path, data);
    return false;
}

bool save(const Path &path, const CurveData &data)
{
    if (path.testExtension("hair"))
        return saveHair(path, data);
    else if (path.testExtension("mitshair"))
        return saveMitsubaHair(path, data);
    else if (path.testExtension("fiber"))
        return Fiber::saveFiber(path, data);
    return false;
}

}

}
