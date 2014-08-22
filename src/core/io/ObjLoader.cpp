#include <algorithm>
#include <iostream>
#include <sstream>
#include <cctype>

#include "ObjLoader.hpp"
#include "FileUtils.hpp"
#include "Scene.hpp"

#include "primitives/Sphere.hpp"
#include "primitives/Quad.hpp"
#include "primitives/Mesh.hpp"

#include "cameras/PinholeCamera.hpp"

#include "bsdfs/RoughConductorBsdf.hpp"
#include "bsdfs/RoughPlasticBsdf.hpp"
#include "bsdfs/TransparencyBsdf.hpp"
#include "bsdfs/DielectricBsdf.hpp"
#include "bsdfs/OrenNayarBsdf.hpp"
#include "bsdfs/ThinSheetBsdf.hpp"
#include "bsdfs/LambertBsdf.hpp"
#include "bsdfs/MirrorBsdf.hpp"
#include "bsdfs/PhongBsdf.hpp"
#include "bsdfs/MixedBsdf.hpp"
#include "bsdfs/ErrorBsdf.hpp"

namespace Tungsten {

template<unsigned Size>
Vec<float, Size> ObjLoader::loadVector(const char *s)
{
    std::istringstream ss(s);
    Vec<float, Size> result;
    for (int i = 0; i < Size && !ss.eof() && !ss.fail(); ++i)
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

int32 ObjLoader::addVertex(int32 pos, int32 normal, int32 uv)
{
    if (pos < 0)
        pos += _pos.size() + 1;
    if (normal < 0)
        normal += _normal.size() + 1;
    if (uv < 0)
        uv += _uv.size() + 1;

    // Absolutely bullet proof. I promise.
    uint64 hash = (uint64(pos) << 32) ^ (uint64(normal) << 16) ^ uint64(uv);

    /*if (_indices.count(hash)) {
        return _indices[hash];
    } else {*/
        Vec3f p, n;
        Vec2f u;

        if (pos)
            p = _pos[pos - 1];
        if (normal)
            n = _normal[normal - 1];
        if (uv)
            u = _uv[uv - 1];

        _bounds.grow(p);

        uint32 index = _verts.size();
        _verts.emplace_back(p, n, u);

        _indices[hash] = index;
        return index;
    //}
}

void ObjLoader::loadFace(const char *line)
{
    int32 first = -1, current = -1;

    std::istringstream ss(line);
    while (!ss.fail() && !ss.eof()) {
        int32 indices[] = {0, 0, 0};
        for (int i = 0; i < 3; ++i) {
            if (ss.peek() != '/')
                ss >> indices[i];
            if (ss.peek() == '/') {
                ss.get();
            } else {
                break;
            }
        }
        if (indices[0] == 0)
            break;

        int32 vert = addVertex(indices[0], indices[2], indices[1]);

        if (first != -1)
            _tris.emplace_back(first, current, vert, _currentMaterial);
        else
            first = current;
        current = vert;
    }
}

std::string ObjLoader::extractString(const char *line)
{
    std::string str(line); /* TODO */
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

    std::ifstream in(mtlPath.c_str(), std::ios::in | std::ios::binary);

    if (in.good()) {
        size_t previousTop = _materials.size();

        std::string strLine;
        uint32 matIndex = -1;
        while (!in.eof() && !in.fail()) {
            std::getline(in, strLine);
            const char *line = strLine.c_str();
            skipWhitespace(line);

            if (hasPrefix(line, "newmtl")) {
                matIndex = _materials.size();
                std::string name = extractString(line + 7);
                _materialToIndex[name] = matIndex;
                _materials.push_back(ObjMaterial(name));
                DBG("Loaded material %s", name);
            } else if (hasPrefix(line, "Kd")) {
                _materials[matIndex].diffuse = loadVector<3>(line + 3);
                //std::cout << "Diffuse: " << _materials[matIndex].diffuse << std::endl;
            } else if (hasPrefix(line, "Ks")) {
                _materials[matIndex].specular = loadVector<3>(line + 3);
                //std::cout << "Specular: " << _materials[matIndex].specular << std::endl;
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
        std::cerr << "Unable to load material library at '" << mtlPath << "'" << std::endl;
    }
}

void ObjLoader::loadLine(const char *line)
{
    bool meshBoundary =
               hasPrefix(line, "usemtl")
            || hasPrefix(line, "g")
            || hasPrefix(line, "o")
            || hasPrefix(line, "s");
    //bool meshBoundary = hasPrefix(line, "o");

    if (meshBoundary && !_tris.empty()) {
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
    else if (hasPrefix(line, "mtllib"))
        loadMaterialLibrary(line + 7);
    else if (hasPrefix(line, "usemtl")) {
        meshBoundary = true;
        std::string mtlName = extractString(line + 7);
        if (_materialToIndex.count(mtlName))
            _currentMaterial = _materialToIndex[mtlName];
        else {
            DBG("Could not load material %s", mtlName);
            _currentMaterial = -1;
        }
    } else if (hasPrefix(line, "g") || hasPrefix(line, "o")) {
        meshBoundary = true;
        _meshName = extractString(line + 2);
    } else if (hasPrefix(line, "s")) {
        meshBoundary = true;
        if (extractString(line + 2) == "off")
            _meshSmoothed = false;
        else
            _meshSmoothed = true;
    }
}

std::shared_ptr<TextureRgb> ObjLoader::fetchColorMap(const std::string &path)
{
    if (path.empty())
        return nullptr;

    if (_colorMaps.count(path))
        return _colorMaps[path];

    std::shared_ptr<BitmapTextureRgb> tex(BitmapTextureUtils::loadColorTexture(path));
    _colorMaps[path] = tex;
    return tex;
}

std::shared_ptr<TextureA> ObjLoader::fetchScalarMap(const std::string &path)
{
    if (path.empty())
        return nullptr;

    if (_scalarMaps.count(path))
        return _scalarMaps[path];

    std::shared_ptr<BitmapTextureA> tex(BitmapTextureUtils::loadScalarTexture(path));
    _scalarMaps[path] = tex;
    return tex;
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
            result->setAlbedo(std::make_shared<ConstantTextureRgb>(mat.diffuse));
        } else if (mat.hardness > 500.0f) {
            result = std::make_shared<MirrorBsdf>();
            result->setAlbedo(std::make_shared<ConstantTextureRgb>(mat.specular));
        } else {
            std::shared_ptr<Bsdf> lambert = std::make_shared<LambertBsdf>();
            std::shared_ptr<Bsdf> phong = std::make_shared<PhongBsdf>(int(mat.hardness));
            lambert->setAlbedo(std::make_shared<ConstantTextureRgb>(mat.diffuse));
            phong->setAlbedo(std::make_shared<ConstantTextureRgb>(mat.specular));
            float ratio = mat.diffuse.max()/(mat.specular.max() + mat.diffuse.max());
            result = std::make_shared<MixedBsdf>(lambert, phong, ratio);
        }
    } else {
        result = std::make_shared<DielectricBsdf>(mat.ior);
    }
    if (!result)
        return _errorMaterial;

    // TODO: Can no longer set bump maps directly, since they moved from Bsdf to Primitive.
    // Need to somehow remember this piece of info for later so we can set the bump map on
    // the primitive when the bsdf is applied
//  if (mat.hasBumpMap())
//      result->setBump(fetchScalarMap(mat.bumpMap));
    if (mat.hasDiffuseMap())
        result->setAlbedo(fetchColorMap(mat.diffuseMap));
    if (mat.hasAlphaMap())
        result = std::make_shared<TransparencyBsdf>(fetchColorMap(mat.alphaMap), result);

    result->setName(mat.name);

    return result;
}

std::string ObjLoader::generateDummyName() const
{
    std::stringstream ss;
    ss << "Mesh" << _meshes.size() + 1;

    return std::move(ss.str());
}

void ObjLoader::clearPerMeshData()
{
    _meshName.clear();
    _indices.clear();
    _tris.clear();
    _verts.clear();
}

std::shared_ptr<Primitive> ObjLoader::finalizeMesh()
{
    std::shared_ptr<TextureRgb> emission;
    std::shared_ptr<Bsdf> bsdf;
    if (_currentMaterial == -1) {
        bsdf = _errorMaterial;
    } else {
        bsdf = _convertedMaterials[_currentMaterial];
        if (_materials[_currentMaterial].isEmissive())
            emission = std::make_shared<ConstantTextureRgb>(_materials[_currentMaterial].emission);
    }

    std::string name = _meshName.empty() ? generateDummyName() : _meshName;

    std::shared_ptr<Primitive> prim;
    if (name.find("AnalyticSphere") != std::string::npos) {
        Vec3f center(0.0f);
        for (const Vertex &v : _verts)
            center += v.pos()/_verts.size();
        float r = 0.0f;
        for (const Vertex &v : _verts)
            r = max(r, (center - v.pos()).length());
        prim = std::make_shared<Sphere>(center, r, name, bsdf);
    } else if (name.find("AnalyticQuad") != std::string::npos) {
        if (_tris.size() == 2) {
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

            prim = std::make_shared<Quad>(base, edge0, edge1, name, bsdf);
        } else {
            DBG("AnalyticQuad must have exactly 2 triangles. Mesh '%s' has %d instead", _meshName.c_str(), _tris.size());
        }
    }

    if (!prim)
        prim = std::make_shared<TriangleMesh>(std::move(_verts), std::move(_tris), bsdf, name, _meshSmoothed);

    prim->setEmission(emission);

    return prim;
}

ObjLoader::ObjLoader(std::ifstream &in, const char *path)
: _errorMaterial(std::make_shared<ErrorBsdf>()),
  _currentMaterial(-1),
  _meshSmoothed(false)
{
    std::string previousDir = FileUtils::getCurrentDir();

    _folder = FileUtils::extractDir(std::string(path));
    if (!_folder.empty()) {
        FileUtils::changeCurrentDir(_folder);
        _folder.append("/");
    }

    std::string line;
    while (!in.fail() && !in.eof()) {
        std::getline(in, line);

        loadLine(line.c_str());
    }

    if (!_tris.empty()) {
        _meshes.emplace_back(finalizeMesh());
        clearPerMeshData();
    }

    FileUtils::changeCurrentDir(previousDir);
}

Scene *ObjLoader::load(const char *path)
{
    std::ifstream file(path, std::ios::in | std::ios::binary);

    if (file.good()) {
        ObjLoader loader(file, path);

        std::shared_ptr<Camera> cam(std::make_shared<PinholeCamera>());
        cam->setLookAt(loader._bounds.center());
        cam->setPos(loader._bounds.center() - Vec3f(0.0f, 0.0f, loader._bounds.diagonal().z()));

        return new Scene(
            FileUtils::extractDir(path),
            std::move(loader._meshes),
            std::vector<std::shared_ptr<Bsdf>>(),
            cam
        );
    } else {
        return nullptr;
    }
}

}
