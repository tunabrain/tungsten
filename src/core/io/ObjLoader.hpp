#ifndef OBJLOADER_HPP_
#define OBJLOADER_HPP_

#include "TextureCache.hpp"
#include "ObjMaterial.hpp"

#include "primitives/Triangle.hpp"
#include "primitives/Vertex.hpp"

#include "bsdfs/Bsdf.hpp"

#include "math/Vec.hpp"
#include "math/Box.hpp"

#include <unordered_map>
#include <fstream>
#include <string>
#include <vector>
#include <memory>

namespace Tungsten {

class Primitive;

class ObjLoader
{
    bool _geometryOnly;

    std::shared_ptr<Bsdf> _errorMaterial;
    std::vector<ObjMaterial> _materials;
    std::unordered_map<std::string, uint32> _materialToIndex;
    std::vector<std::shared_ptr<Bsdf>> _convertedMaterials;
    std::shared_ptr<TextureCache> _textureCache;
    int32 _currentMaterial;

    std::vector<Vec3f> _pos;
    std::vector<Vec3f> _normal;
    std::vector<Vec2f> _uv;

    std::string _meshName;
    bool _meshSmoothed;

    std::unordered_map<Vec3i, uint32> _indices;
    std::vector<TriangleI> _tris;
    std::vector<Vertex> _verts;
    Box3f _bounds;

    std::vector<std::shared_ptr<Primitive>> _meshes;

    void skipWhitespace(const char *&s);
    bool hasPrefix(const char *s, const char *pre);
    uint32 fetchVertex(int32 pos, int32 normal, int32 uv);

    std::string extractString(const char *line);
    std::string extractPath(const char *line);

    template<unsigned Size>
    Vec<float, Size> loadVector(const char *s);
    void loadFace(const char *line);
    void loadMaterialLibrary(const char *path);
    void loadLine(const char *line);
    void loadFile(std::ifstream &in);

    std::shared_ptr<Bsdf> convertObjMaterial(const ObjMaterial &mat);

    std::string generateDummyName() const;
    void clearPerMeshData();

    std::shared_ptr<Primitive> tryInstantiateSphere(const std::string &name, std::shared_ptr<Bsdf> &bsdf);
    std::shared_ptr<Primitive> tryInstantiateQuad(const std::string &name, std::shared_ptr<Bsdf> &bsdf);
    std::shared_ptr<Primitive> finalizeMesh();

    ObjLoader(std::ifstream &in, const std::string &path, std::shared_ptr<TextureCache> cache);
    ObjLoader(std::ifstream &in);

public:
    static Scene *load(const std::string &path, std::shared_ptr<TextureCache> cache = nullptr);
    static bool loadGeometryOnly(const std::string &path, std::vector<Vertex> &verts, std::vector<TriangleI> &tris);
};

}

#endif /* OBJLOADER_HPP_ */
