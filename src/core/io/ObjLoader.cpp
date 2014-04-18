#include <algorithm>
#include <iostream>
#include <sstream>
#include <cctype>

#include "ObjLoader.hpp"
#include "FileUtils.hpp"
#include "Scene.hpp"

#include "primitives/Mesh.hpp"

#include "bsdfs/DielectricBsdf.hpp"
#include "bsdfs/LambertBsdf.hpp"
#include "bsdfs/PhongBsdf.hpp"
#include "bsdfs/MixedBsdf.hpp"

#include "Camera.hpp"

namespace Tungsten
{


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
                std::cout << "Loaded material " << name << std::endl;
            } else if (hasPrefix(line, "Kd")) {
                _materials[matIndex].diffuse = loadVector<3>(line + 3);
                std::cout << "Diffuse: " << _materials[matIndex].diffuse << std::endl;
            } else if (hasPrefix(line, "Ks")) {
                _materials[matIndex].specular = loadVector<3>(line + 3);
                std::cout << "Specular: " << _materials[matIndex].specular << std::endl;
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
            std::cout << "Could not load material " << mtlName << std::endl;
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

std::shared_ptr<TextureRgba> ObjLoader::fetchColorMap(const std::string &path)
{
    if (path.empty())
        return nullptr;

    if (_colorMaps.count(path))
        return _colorMaps[path];

    std::shared_ptr<TextureRgba> tex(new TextureRgba(path));
    _colorMaps[path] = tex;
    return tex;
}

std::shared_ptr<TextureA> ObjLoader::fetchScalarMap(const std::string &path)
{
    if (path.empty())
        return nullptr;

    if (_scalarMaps.count(path))
        return _scalarMaps[path];

    std::shared_ptr<TextureA> tex(new TextureA(path));
    _scalarMaps[path] = tex;
    return tex;
}

Material *ObjLoader::convertObjMaterial(const ObjMaterial &mat)
{
    std::shared_ptr<Bsdf> bsdf = nullptr;

    if (!mat.isTransmissive()) {
        if (!mat.isSpecular()) {
            bsdf = std::shared_ptr<Bsdf>(new LambertBsdf(mat.hasDiffuseMap() ? Vec3f(1.0f) : mat.diffuse));
        } else {
            bsdf = std::shared_ptr<Bsdf>(new MixedBsdf(
                std::shared_ptr<Bsdf>(new LambertBsdf(mat.hasDiffuseMap() ? Vec3f(1.0f) : mat.diffuse)),
                std::shared_ptr<Bsdf>(new PhongBsdf(mat.hasDiffuseMap() ? Vec3f(1.0f) : mat.specular, int(mat.hardness))),
                mat.diffuse.max()*(mat.hardness + 1.0f)/(mat.specular.max()*2.0f + mat.diffuse.max()*(mat.hardness + 1.0f))
            ));
        }
    } else {
        bsdf = std::shared_ptr<Bsdf>(new DielectricBsdf(mat.ior, mat.hasDiffuseMap() ? Vec3f(1.0f) : mat.diffuse, 1));
    }

    if (bsdf)
        return new Material(bsdf, mat.emission, mat.name, fetchColorMap(mat.diffuseMap),
                fetchScalarMap(mat.alphaMap), fetchScalarMap(mat.bumpMap));
    else
        return new Material(*_errorMaterial);
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

TriangleMesh *ObjLoader::finalizeMesh()
{
    std::string name = _meshName.empty() ? generateDummyName() : _meshName;

    return new TriangleMesh(
        std::move(_verts),
        std::move(_tris),
        _currentMaterial == -1 ? _errorMaterial : _convertedMaterials[_currentMaterial],
        name,
        _meshSmoothed
    );
}

ObjLoader::ObjLoader(std::ifstream &in, const char *path)
: _errorMaterial(new Material(std::shared_ptr<Bsdf>(new LambertBsdf(Vec3f(0.0f))), Vec3f(1.0f, 0.0f, 0.0f), "Undefined")),
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

        return new Scene(
            FileUtils::extractDir(path),
            std::move(loader._meshes),
            std::move(loader._convertedMaterials),
            std::vector<std::shared_ptr<Light>>(),
            std::vector<std::shared_ptr<Bsdf>>(),
            std::shared_ptr<Camera>(new Camera(Mat4f::translate(Vec3f(0.0f)), Vec2u(800u, 600u), 60.0f, 32))
        );
    } else {
        return nullptr;
    }
}

}
