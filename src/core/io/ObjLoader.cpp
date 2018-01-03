#include "ObjLoader.hpp"
#include "DirectoryChange.hpp"
#include "FileUtils.hpp"
#include "Scene.hpp"

#include "primitives/TriangleMesh.hpp"
#include "primitives/Sphere.hpp"
#include "primitives/Curves.hpp"
#include "primitives/Cube.hpp"
#include "primitives/Quad.hpp"
#include "primitives/Disk.hpp"

#include "textures/ConstantTexture.hpp"
#include "textures/BitmapTexture.hpp"

#include "cameras/PinholeCamera.hpp"

#include "bsdfs/RoughConductorBsdf.hpp"
#include "bsdfs/RoughPlasticBsdf.hpp"
#include "bsdfs/TransparencyBsdf.hpp"
#include "bsdfs/DielectricBsdf.hpp"
#include "bsdfs/OrenNayarBsdf.hpp"
#include "bsdfs/ThinSheetBsdf.hpp"
#include "bsdfs/MirrorBsdf.hpp"
#include "bsdfs/PhongBsdf.hpp"
#include "bsdfs/ErrorBsdf.hpp"

#include <tinyformat/tinyformat.hpp>
#include <algorithm>
#include <cctype>

namespace Tungsten {

template<unsigned Size>
Vec<float, Size> ObjLoader::loadVector(const char *s)
{
    std::istringstream ss(s);
    Vec<float, Size> result;
    for (unsigned i = 0; i < Size && !ss.eof() && !ss.fail(); ++i)
        ss >> result[i];
    return result;
}

void ObjLoader::skipWhitespace(const char *&s)
{
    while (std::isspace(*s))
        s++;
}

bool ObjLoader::hasPrefix(const char *s, const char *pre)
{
    do {
        if (tolower(*pre) != tolower(*s))
            return false;
    } while (*++s && *++pre);
    return *pre == '\0' && (isspace(*s) || *s == '\0');
}

uint32 ObjLoader::fetchVertex(int32 pos, int32 normal, int32 uv)
{
    if (pos < 0)
        pos += _pos.size() + 1;
    if (normal < 0)
        normal += _normal.size() + 1;
    if (uv < 0)
        uv += _uv.size() + 1;

    auto iter = _indices.find(Vec3i(pos, normal, uv));
    if (iter != _indices.end()) {
        return iter->second;
    } else {
        Vec3f p(0.0f), n(0.0f, 1.0f, 0.0f);
        Vec2f u(0.0f);

        if (pos)
            p = _pos[pos - 1];
        if (normal)
            n = _normal[normal - 1];
        if (uv)
            u = _uv[uv - 1];

        _bounds.grow(p);

        uint32 index = _verts.size();
        _verts.emplace_back(p, n, u);

        _indices.insert(std::make_pair(Vec3i(pos, normal, uv), index));
        return index;
    }
}

void ObjLoader::loadCurve(const char *line)
{
    uint32 prev = 0;
    int vertexCount = 0;

    std::istringstream ss(line);
    while (!ss.fail() && !ss.eof()) {
        int32 index;
        ss >> index;
        if (ss.peek() == '/') {
            ss.get();
            uint32 tmp;
            ss >> tmp;
        }

        uint32 current = fetchVertex(index, 0, 0);

        if (++vertexCount >= 2)
            _segments.emplace_back(SegmentI{prev, current});
        prev = current;
    }
}

void ObjLoader::loadFace(const char *line)
{
    uint32 first = 0, current = 0;
    int vertexCount = 0;

    std::istringstream ss(line);
    while (!ss.fail() && !ss.eof()) {
        int32 indices[] = {0, 0, 0};
        for (int i = 0; i < 3; ++i) {
            if (ss.peek() != '/')
                ss >> indices[i];
            if (ss.peek() == '/')
                ss.get();
            else
                break;
        }
        if (indices[0] == 0)
            break;

        uint32 vert = fetchVertex(indices[0], indices[2], indices[1]);

        if (++vertexCount >= 3)
            _tris.emplace_back(first, current, vert, _currentMaterial);
        else
            first = current;
        current = vert;
    }
}

std::string ObjLoader::extractString(const char *line)
{
    std::string str(line);
    auto pos = str.find_first_of("\r\n\t");
    if (pos != std::string::npos)
        str.erase(pos);
    while (str.back() == ' ')
        str.pop_back();

    return str;
}

std::string ObjLoader::extractPath(const char *line)
{
    std::string str(extractString(line));
    std::replace(str.begin(), str.end(), '\\', '/');
    return str;
}

void ObjLoader::loadMaterialLibrary(const char *path)
{
    std::string mtlPath = extractString(path);

    InputStreamHandle in = FileUtils::openInputStream(Path(mtlPath));
    if (in) {
        size_t previousTop = _materials.size();

        std::string strLine;
        uint32 matIndex = -1;
        while (!in->eof() && !in->fail()) {
            std::getline(*in, strLine);
            const char *line = strLine.c_str();
            skipWhitespace(line);

            if (hasPrefix(line, "newmtl")) {
                matIndex = _materials.size();
                std::string name = extractString(line + 7);
                _materialToIndex.insert(std::make_pair(name, matIndex));
                _materials.push_back(ObjMaterial(name));
                DBG("Loaded material %s", name);
            } else if (hasPrefix(line, "Kd")) {
                _materials[matIndex].diffuse = loadVector<3>(line + 3);
            } else if (hasPrefix(line, "Ks")) {
                _materials[matIndex].specular = loadVector<3>(line + 3);
            } else if (hasPrefix(line, "Ke")) {
                _materials[matIndex].emission = loadVector<3>(line + 3);
            } else if (hasPrefix(line, "Tf")) {
                _materials[matIndex].opacity = loadVector<3>(line + 3);
            } else if (hasPrefix(line, "Ns")) {
                _materials[matIndex].hardness = loadVector<1>(line + 3).x();
            } else if (hasPrefix(line, "Ni")) {
                _materials[matIndex].ior = loadVector<1>(line + 3).x();
            } else if (hasPrefix(line, "map_Kd")) {
                _materials[matIndex].diffuseMap = extractPath(line + 7);
            } else if (hasPrefix(line, "map_d")) {
                _materials[matIndex].alphaMap = extractPath(line + 6);
            } else if (hasPrefix(line, "map_bump")) {
                _materials[matIndex].bumpMap = extractPath(line + 9);
            }
        }

        for (size_t i = previousTop; i < _materials.size(); ++i)
            _convertedMaterials.emplace_back(convertObjMaterial(_materials[i]));
    } else {
        DBG("Unable to load material library at '%s'", mtlPath);
    }
}

void ObjLoader::loadLine(const char *line)
{
    bool meshBoundary =
               hasPrefix(line, "usemtl")
            || hasPrefix(line, "g")
            || hasPrefix(line, "o")
            || hasPrefix(line, "s");

    if (meshBoundary && (!_tris.empty() || !_segments.empty()) && !_geometryOnly) {
        _meshes.emplace_back(finalizeMesh());
        clearPerMeshData();
    }

    skipWhitespace(line);
    if (hasPrefix(line, "v"))
        _pos.push_back(loadVector<3>(line + 2));
    else if (hasPrefix(line, "vn"))
        _normal.push_back(loadVector<3>(line + 3));
    else if (hasPrefix(line, "vt"))
        _uv.push_back(loadVector<2>(line + 3));
    else if (hasPrefix(line, "f"))
        loadFace(line + 2);
    else if (hasPrefix(line, "l"))
        loadCurve(line + 2);
    else if (_geometryOnly)
        return;
    else if (hasPrefix(line, "mtllib"))
        loadMaterialLibrary(line + 7);
    else if (hasPrefix(line, "usemtl")) {
        std::string mtlName = extractString(line + 7);
        auto iter = _materialToIndex.find(mtlName);
        if (iter != _materialToIndex.end())
            _currentMaterial = iter->second;
        else {
            DBG("Could not load material %s", mtlName);
            _currentMaterial = -1;
        }
    } else if (hasPrefix(line, "g") || hasPrefix(line, "o")) {
        _meshName = extractString(line + 2);
    } else if (hasPrefix(line, "s")) {
        if (extractString(line + 2) == "off")
            _meshSmoothed = false;
        else
            _meshSmoothed = true;
    }
}

std::shared_ptr<Bsdf> ObjLoader::convertObjMaterial(const ObjMaterial &mat)
{
    std::shared_ptr<Bsdf> result = nullptr;

    if (mat.name.find("Thinsheet") != std::string::npos) {
        result = std::make_shared<ThinSheetBsdf>();
    } else if (mat.name.find("OrenNayar") != std::string::npos) {
        result = std::make_shared<OrenNayarBsdf>();
    } else if (mat.name.find("RoughConductor") != std::string::npos) {
        result = std::make_shared<RoughConductorBsdf>();
    } else if (mat.name.find("RoughPlastic") != std::string::npos) {
        result = std::make_shared<RoughPlasticBsdf>();
    } else if (!mat.isTransmissive()) {
        if (!mat.isSpecular()) {
            result = std::make_shared<LambertBsdf>();
            result->setAlbedo(std::make_shared<ConstantTexture>(mat.diffuse));
        } else if (mat.hardness > 500.0f) {
            result = std::make_shared<MirrorBsdf>();
            result->setAlbedo(std::make_shared<ConstantTexture>(mat.specular));
        } else {
            float diffuseRatio = mat.diffuse.max()/(mat.specular.max() + mat.diffuse.max());
            result = std::make_shared<PhongBsdf>(mat.hardness, diffuseRatio);
            result->setAlbedo(std::make_shared<ConstantTexture>(lerp(mat.specular, mat.diffuse, diffuseRatio)));        }
    } else {
        result = std::make_shared<DielectricBsdf>(mat.ior);
    }
    if (!result)
        return _errorMaterial;

    if (mat.hasDiffuseMap()) {
        PathPtr path = std::make_shared<Path>(mat.diffuseMap);
        path->freezeWorkingDirectory();

        auto texture = _textureCache->fetchTexture(path, TexelConversion::REQUEST_RGB);
        if (texture)
            result->setAlbedo(texture);
    }
    if (mat.hasAlphaMap()) {
        PathPtr path = std::make_shared<Path>(mat.alphaMap);
        path->freezeWorkingDirectory();

        auto texture = _textureCache->fetchTexture(path, TexelConversion::REQUEST_AUTO);
        if (texture)
            result = std::make_shared<TransparencyBsdf>(texture, result);
    }
    if (mat.hasBumpMap()) {
        PathPtr path = std::make_shared<Path>(mat.alphaMap);
        path->freezeWorkingDirectory();

        auto texture = _textureCache->fetchTexture(path, TexelConversion::REQUEST_AVERAGE);
        if (texture)
            result->setBump(texture);
    }

    result->setName(mat.name);

    return result;
}

std::string ObjLoader::generateDummyName() const
{
    return tfm::format("Mesh%d", _meshes.size() + 1);
}

void ObjLoader::clearPerMeshData()
{
    _meshName.clear();
    _indices.clear();
    _tris.clear();
    _verts.clear();
    _segments.clear();
}

void ObjLoader::finalizeCurveData(std::vector<uint32> &curveEnds, std::vector<Vec4f> &nodeData)
{
    std::unique_ptr<uint32[]> pred(new uint32[_verts.size()]);
    std::unique_ptr<uint32[]> succ(new uint32[_verts.size()]);
    std::memset(pred.get(), 0, _verts.size()*sizeof(uint32));
    std::memset(succ.get(), 0, _verts.size()*sizeof(uint32));

    for (const SegmentI &s : _segments) {
        pred[s.v1] = s.v0 + 1;
        succ[s.v0] = s.v1 + 1;
    }
    int numCurves = 0, numNodes = 0;
    for (const SegmentI &s : _segments) {
        if (succ[s.v1] == 0)
            numCurves++;
        numNodes++;
    }
    numNodes += numCurves*3;

    curveEnds.resize(numCurves);
    nodeData.resize(numNodes);

    float width = 0.01f;
    uint32 nodeIdx = 0, curveIdx = 0;
    for (const SegmentI &s : _segments) {
        if (pred[s.v0] != 0)
            continue;

        Vec3f p = _verts[s.v0].pos()*2.0f - _verts[s.v1].pos();
        nodeData[nodeIdx++] = Vec4f(p.x(), p.y(), p.z(), width);
        uint32 vert = s.v0 + 1;
        while (vert) {
            Vec3f p = _verts[vert - 1].pos();
            nodeData[nodeIdx++] = Vec4f(p.x(), p.y(), p.z(), width);
            vert = succ[vert - 1];
        }
        p = nodeData[nodeIdx - 1].xyz()*2.0f - nodeData[nodeIdx - 2].xyz();
        nodeData[nodeIdx++] = Vec4f(p.x(), p.y(), p.z(), width);

        curveEnds[curveIdx++] = nodeIdx;
    }
}

std::shared_ptr<Primitive> ObjLoader::tryInstantiateSphere(const std::string &name, std::shared_ptr<Bsdf> &bsdf)
{
    Vec3f center(0.0f);
    for (const Vertex &v : _verts)
        center += v.pos()/_verts.size();
    float r = 0.0f;
    for (const Vertex &v : _verts)
        r = max(r, (center - v.pos()).length());
    return std::make_shared<Sphere>(center, r, name, bsdf);
}

std::shared_ptr<Primitive> ObjLoader::tryInstantiateQuad(const std::string &name, std::shared_ptr<Bsdf> &bsdf)
{
    if (_tris.size() != 2) {
        DBG("AnalyticQuad must have exactly 2 triangles. Mesh '%s' has %d instead", _meshName.c_str(), _tris.size());
        return nullptr;
    }

    TriangleI &t = _tris[0];
    Vec3f p0 = _verts[t.v0].pos();
    Vec3f p1 = _verts[t.v1].pos();
    Vec3f p2 = _verts[t.v2].pos();
    float absDot0 = std::abs((p1 - p0).dot(p2 - p0));
    float absDot1 = std::abs((p2 - p1).dot(p0 - p1));
    float absDot2 = std::abs((p0 - p2).dot(p1 - p2));
    Vec3f base, edge0, edge1;
    if (absDot0 < absDot1 && absDot0 < absDot2)
        base = p0, edge0 = p1 - base, edge1 = p2 - base;
    else if (absDot1 < absDot2)
        base = p1, edge0 = p2 - base, edge1 = p0 - base;
    else
        base = p2, edge0 = p0 - base, edge1 = p1 - base;

    return std::make_shared<Quad>(base, edge0, edge1, name, bsdf);
}

std::shared_ptr<Primitive> ObjLoader::tryInstantiateCube(const std::string &name, std::shared_ptr<Bsdf> &bsdf)
{
    if (_tris.size() != 12) {
        DBG("AnalyticCube must have exactly 12 triangles. Mesh '%s' has %d instead", _meshName.c_str(), _tris.size());
        return nullptr;
    }

    TriangleI &t = _tris[0];
    Vec3f p0 = _verts[t.v0].pos();
    Vec3f p1 = _verts[t.v1].pos();
    Vec3f p2 = _verts[t.v2].pos();
    float absDot0 = std::abs((p1 - p0).dot(p2 - p0));
    float absDot1 = std::abs((p2 - p1).dot(p0 - p1));
    float absDot2 = std::abs((p0 - p2).dot(p1 - p2));
    Vec3f base, edge0, edge1;
    if (absDot0 < absDot1 && absDot0 < absDot2)
        base = p0, edge0 = p1 - base, edge1 = p2 - base;
    else if (absDot1 < absDot2)
        base = p1, edge0 = p2 - base, edge1 = p0 - base;
    else
        base = p2, edge0 = p0 - base, edge1 = p1 - base;

    float maxDistSq = 0.0f;
    Vec3f outOfPlane(base);
    for (int i = 1; i < 12; ++i) {
        for (int t = 0; t < 3; ++t) {
            float distSq = (_verts[_tris[i].vs[t]].pos() - base).lengthSq();
            if (distSq > maxDistSq) {
                maxDistSq = distSq;
                outOfPlane = _verts[_tris[i].vs[t]].pos();
            }
        }
    }
    Vec3f edge2 = outOfPlane - base;

    // Gram-Schmidt
    edge1 -= edge0*(edge1.dot(edge0)/edge0.lengthSq());
    edge2 -= edge0*(edge2.dot(edge0)/edge0.lengthSq());
    edge2 -= edge1*(edge2.dot(edge1)/edge1.lengthSq());

    Vec3f pos = base + (edge0 + edge1 + edge2)*0.5f;
    Vec3f scale = Vec3f(edge0.length(), edge1.length(), edge2.length());
    Mat4f rot(edge0.normalized(), edge1.normalized(), edge2.normalized());

    return std::make_shared<Cube>(pos, scale, rot, name, bsdf);
}

std::shared_ptr<Primitive> ObjLoader::tryInstantiateDisk(const std::string &name, std::shared_ptr<Bsdf> &bsdf)
{
    Vec3f n(0.0f);
    for (size_t i = 0; i < _tris.size(); ++i) {
        Vec3f p0 = _verts[_tris[i].v0].pos();
        Vec3f p1 = _verts[_tris[i].v1].pos();
        Vec3f p2 = _verts[_tris[i].v2].pos();
        n += (p1 - p0).cross(p2 - p0);
    }
    n.normalize();

    Vec3f center(0.0f);
    for (size_t i = 0; i < _verts.size(); ++i)
        center += _verts[i].pos()*(1.0f/_verts.size());

    float r = 0.0f;
    for (size_t i = 0; i < _verts.size(); ++i)
        r = max(r, (_verts[i].pos() - center).length());

    return std::make_shared<Disk>(center, n, r, name, bsdf);
}

std::shared_ptr<Primitive> ObjLoader::finalizeMesh()
{
    std::shared_ptr<Texture> emission;
    std::shared_ptr<Bsdf> bsdf;
    if (_currentMaterial == -1) {
        bsdf = _errorMaterial;
    } else {
        bsdf = _convertedMaterials[_currentMaterial];

        ObjMaterial &mat = _materials[_currentMaterial];
        if (mat.isEmissive())
            emission = std::make_shared<ConstantTexture>(mat.emission);
    }

    std::string name = _meshName.empty() ? generateDummyName() : _meshName;

    std::shared_ptr<Primitive> prim;
    if (name.find("AnalyticSphere") != std::string::npos)
        prim = tryInstantiateSphere(name, bsdf);
    else if (name.find("AnalyticQuad") != std::string::npos)
        prim = tryInstantiateQuad(name, bsdf);
    else if (name.find("AnalyticCube") != std::string::npos)
        prim = tryInstantiateCube(name, bsdf);
    else if (name.find("AnalyticDisk") != std::string::npos)
        prim = tryInstantiateDisk(name, bsdf);

    if (!prim && !_segments.empty() && _tris.empty()) {
        std::vector<uint32> curveEnds;
        std::vector<Vec4f> nodeData;
        finalizeCurveData(curveEnds, nodeData);
        prim = std::make_shared<Curves>(std::move(curveEnds), std::move(nodeData), bsdf, name);
    } else if (!prim) {
        prim = std::make_shared<TriangleMesh>(std::move(_verts), std::move(_tris), bsdf, name, _meshSmoothed, false);
    }

    prim->setEmission(emission);

    return std::move(prim);
}

void ObjLoader::loadFile(std::istream &in)
{
    std::string line;
    while (!in.fail() && !in.eof()) {
        std::getline(in, line);

        loadLine(line.c_str());
    }
}

ObjLoader::ObjLoader(std::istream &in, const Path &path, std::shared_ptr<TextureCache> cache)
: _geometryOnly(false),
  _errorMaterial(std::make_shared<ErrorBsdf>()),
  _textureCache(std::move(cache)),
  _currentMaterial(-1),
  _meshSmoothed(false)
{
    DirectoryChange context(path.parent());

    loadFile(in);

    if (!_tris.empty() || !_segments.empty()) {
        _meshes.emplace_back(finalizeMesh());
        clearPerMeshData();
    }
}

ObjLoader::ObjLoader(std::istream &in)
: _geometryOnly(true)
{
    loadFile(in);
}

Scene *ObjLoader::load(const Path &path, std::shared_ptr<TextureCache> cache)
{

    InputStreamHandle file = FileUtils::openInputStream(path);

    if (file) {
        if (!cache)
            cache = std::make_shared<TextureCache>();

        ObjLoader loader(*file, path, cache);

        std::shared_ptr<Camera> cam(std::make_shared<PinholeCamera>());
        cam->setLookAt(loader._bounds.center());
        cam->setPos(loader._bounds.center() - Vec3f(0.0f, 0.0f, loader._bounds.diagonal().z()));

        cache->loadResources();

        return new Scene(
            path.parent(),
            std::move(loader._meshes),
            std::move(loader._convertedMaterials),
            std::move(cache),
            cam
        );
    } else {
        return nullptr;
    }
}

bool ObjLoader::loadGeometryOnly(const Path &path, std::vector<Vertex> &verts, std::vector<TriangleI> &tris)
{
    InputStreamHandle file = FileUtils::openInputStream(path);
    if (!file)
        return false;

    ObjLoader loader(*file);

    verts = std::move(loader._verts);
    tris  = std::move(loader._tris);

    return true;
}

bool ObjLoader::loadCurvesOnly(const Path &path, std::vector<uint32> &curveEnds, std::vector<Vec4f> &nodeData)
{
    InputStreamHandle file = FileUtils::openInputStream(path);
    if (!file)
        return false;

    ObjLoader loader(*file);

    loader.finalizeCurveData(curveEnds, nodeData);

    return true;
}

}
