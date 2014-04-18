#ifndef OBJLOADER_HPP_
#define OBJLOADER_HPP_

#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <map>

#include "ObjMaterial.hpp"

#include "primitives/PackedGeometry.hpp"
#include "primitives/Triangle.hpp"
#include "primitives/Vertex.hpp"

#include "materials/Material.hpp"

#include "math/Vec.hpp"

namespace Tungsten
{

class TriangleMesh;
class Material;

class ObjLoader
{
    std::string _folder;

    std::shared_ptr<Material> _errorMaterial;
    std::vector<ObjMaterial> _materials;
    std::map<std::string, uint32> _materialToIndex;
    std::vector<std::shared_ptr<Material>> _convertedMaterials;
    std::map<std::string, std::shared_ptr<TextureRgba>> _colorMaps;
    std::map<std::string, std::shared_ptr<TextureA>> _scalarMaps;
    int32 _currentMaterial;

    std::vector<Vec3f> _pos;
    std::vector<Vec3f> _normal;
    std::vector<Vec2f> _uv;

    std::string _meshName;
    bool _meshSmoothed;
    std::map<uint64, uint32> _indices;
    std::vector<TriangleI> _tris;
    std::vector<Vertex> _verts;

    std::vector<std::shared_ptr<TriangleMesh>> _meshes;

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

    std::shared_ptr<TextureRgba> fetchColorMap(const std::string &path);
    std::shared_ptr<TextureA> fetchScalarMap(const std::string &path);

    Material *convertObjMaterial(const ObjMaterial &mat);

    std::string generateDummyName() const;
    void clearPerMeshData();
    TriangleMesh *finalizeMesh();

    ObjLoader(std::ifstream &in, const char *path);

public:
    static Scene *load(const char *path);
};

}

#endif /* OBJLOADER_HPP_ */
