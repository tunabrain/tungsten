#ifndef OBJLOADER_HPP_
#define OBJLOADER_HPP_

#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <map>

#include "ObjMaterial.hpp"

#include "primitives/Triangle.hpp"
#include "primitives/Vertex.hpp"

#include "materials/BitmapTexture.hpp"

#include "bsdfs/Bsdf.hpp"

#include "math/Vec.hpp"
#include "math/Box.hpp"

namespace Tungsten {

class Primitive;

class ObjLoader
{
    std::string _folder;

    std::shared_ptr<Bsdf> _errorMaterial;
    std::vector<ObjMaterial> _materials;
    std::map<std::string, uint32> _materialToIndex;
    std::vector<std::shared_ptr<Bsdf>> _convertedMaterials;
    std::map<std::string, std::shared_ptr<BitmapTextureRgb>> _colorMaps;
    std::map<std::string, std::shared_ptr<BitmapTextureA>> _scalarMaps;
    int32 _currentMaterial;

    std::vector<Vec3f> _pos;
    std::vector<Vec3f> _normal;
    std::vector<Vec2f> _uv;

    std::string _meshName;
    bool _meshSmoothed;
    std::map<uint64, uint32> _indices;
    std::vector<TriangleI> _tris;
    std::vector<Vertex> _verts;
    Box3f _bounds;

    std::vector<std::shared_ptr<Primitive>> _meshes;

    void skipWhitespace(const char *&s);
    bool hasPrefix(const char *s, const char *pre);
    int32 addVertex(int32 pos, int32 normal, int32 uv);

    std::string extractString(const char *line);
    std::string extractPath(const char *line);

    template<unsigned Size>
    Vec<float, Size> loadVector(const char *s);

    void loadFace(const char *line);
    void loadMaterialLibrary(const char *path);
    void loadLine(const char *line);

    std::shared_ptr<Texture> fetchBitmap(const std::string &path, bool isScalar);

    std::shared_ptr<Bsdf> convertObjMaterial(const ObjMaterial &mat);

    std::string generateDummyName() const;
    void clearPerMeshData();
    std::shared_ptr<Primitive> finalizeMesh();

    ObjLoader(std::ifstream &in, const char *path);

public:
    static Scene *load(const char *path);
};

}

#endif /* OBJLOADER_HPP_ */
