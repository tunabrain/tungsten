#ifndef HEIGHTMAPTERRAIN_HPP_
#define HEIGHTMAPTERRAIN_HPP_

#include <unordered_map>
#include <memory>

#include "primitives/Triangle.hpp"
#include "primitives/Vertex.hpp"
#include "primitives/Mesh.hpp"

#include "math/MathUtil.hpp"
#include "math/Mat4f.hpp"
#include "math/Vec.hpp"

#include "Camera.hpp"

namespace Tungsten
{

class MinMaxChain
{
    uint32 _w;
    uint32 _h;

    std::unique_ptr<float[]> _base;
    std::vector<std::unique_ptr<Vec2f[]>> _levels;

    Vec2f tileMinMax(uint32 startX, uint32 startY, uint32 endX, uint32 endY) const
    {
        Vec2f minmax(1e30f, -1e30f);

        for (uint32 y = startY; y < endY; ++y)
            for (uint32 x = startX; x < endX; ++x)
                minmax = Vec2f(min(minmax.x(), at(x, y)), max(minmax.y(), at(x, y)));

        return minmax;
    }

    void buildBaseLevel()
    {
        uint32 size = 1 << (_levels.size() - 1);

        for (uint32 y = 0; y < size; ++y) {
            for (uint32 x = 0; x < size; ++x) {
                uint32 startX = (_w*(x + 0))/size;
                uint32 startY = (_h*(y + 0))/size;
                uint32   endX = (_w*(x + 1))/size;
                uint32   endY = (_h*(y + 1))/size;

                at(_levels.size() - 1, x, y) = tileMinMax(startX, startY, endX, endY);
            }
        }
    }

    void downsampleLevel(uint32 level)
    {
        uint32 size  = 1 << level;
        for (uint32 y = 0; y < size; ++y) {
            for (uint32 x = 0; x < size; ++x) {
                Vec2f x00 = at(level + 1, x*2 + 0, y*2 + 0);
                Vec2f x10 = at(level + 1, x*2 + 1, y*2 + 0);
                Vec2f x01 = at(level + 1, x*2 + 0, y*2 + 1);
                Vec2f x11 = at(level + 1, x*2 + 1, y*2 + 1);

                at(level, x, y) = Vec2f(
                    min(x00.x(), x10.x(), x01.x(), x11.x()),
                    max(x00.y(), x10.y(), x01.y(), x11.y())
                );
            }
        }
    }

public:
    MinMaxChain(std::unique_ptr<float[]> base, uint32 w, uint32 h)
    : _w(w), _h(h), _base(std::move(base))
    {
        uint32 levelCount = 1;
        while (2u << levelCount <= min(w, h))
            levelCount++;

        _levels.reserve(levelCount);
        for (uint32 i = 0; i < levelCount; ++i)
            _levels.emplace_back(new Vec2f[1 << 2*i]);

        buildBaseLevel();
        for (int32 i = levelCount - 2; i >= 0; --i)
            downsampleLevel(i);
    }

    Vec2f at(uint32 level, uint32 x, uint32 y) const
    {
        return _levels[level][x + y*(1 << level)];
    }

    Vec2f &at(uint32 level, uint32 x, uint32 y)
    {
        return _levels[level][x + y*(1 << level)];
    }

    void cellPos(uint32 level, uint32 &x, uint32 &y) const
    {
        uint32 size = (1 << level);
        x = ((_w - 1)*x)/size;
        y = ((_h - 1)*y)/size;
    }

    void cellCenter(uint32 level, uint32 &x, uint32 &y) const
    {
        //if (level < _levels.size() - 1) {
            x = x*2 + 1;
            y = y*2 + 1;
            cellPos(level + 1, x, y);
        /*} else {
            uint32 size = (1 << level);
            uint32 startX = ((_w - 1)*(x + 0))/size;
            uint32 startY = ((_h - 1)*(y + 0))/size;
            uint32   endX = ((_w - 1)*(x + 1))/size;
            uint32   endY = ((_h - 1)*(y + 1))/size;

            x = (startX + endX)/2;
            y = (startY + endY)/2;
        }*/
    }

    float at(uint32 x, uint32 y) const
    {
        return _base[x + y*_w];
    }

    float &at(uint32 x, uint32 y)
    {
        return _base[x + y*_w];
    }

    uint32 width() const
    {
        return _w;
    }

    uint32 height() const
    {
        return _h;
    }

    uint32 levels() const
    {
        return _levels.size();
    }
};

class HeightmapTerrain
{
    class TreeNode
    {
        uint32 _level;
        uint32 _x;
        uint32 _y;
        bool _isLeaf;

        std::unique_ptr<TreeNode> _children[4];

    public:
        TreeNode()
        : _level(0), _x(0), _y(0), _isLeaf(true)
        {
        }

        TreeNode(uint32 level, uint32 x, uint32 y)
        : _level(level), _x(x), _y(y), _isLeaf(true)
        {
        }

        uint32 level() const
        {
            return _level;
        }

        uint32 x() const
        {
            return _x;
        }

        uint32 y() const
        {
            return _y;
        }

        bool isLeaf() const
        {
            return _isLeaf;
        }

        TreeNode &operator[](uint32 c)
        {
            return *_children[c];
        }

        const TreeNode &operator[](uint32 c) const
        {
            return *_children[c];
        }

        void split()
        {
            _isLeaf = false;
            _children[0] = std::unique_ptr<TreeNode>(new TreeNode(_level + 1, _x*2 + 0, _y*2 + 0));
            _children[1] = std::unique_ptr<TreeNode>(new TreeNode(_level + 1, _x*2 + 1, _y*2 + 0));
            _children[2] = std::unique_ptr<TreeNode>(new TreeNode(_level + 1, _x*2 + 0, _y*2 + 1));
            _children[3] = std::unique_ptr<TreeNode>(new TreeNode(_level + 1, _x*2 + 1, _y*2 + 1));
        }
    };

    TreeNode _root;
    Mat4f _transform;
    float _maxError;
    uint32 _leafCount;

    const Camera &_camera;
    const MinMaxChain &_heightmap;

    Vec3f globalPos(uint32 x, uint32 y) const
    {
        return _transform*Vec3f(float(x), _heightmap.at(x, y), float(y));
    }

    Vec3f globalCellPos(uint32 level, uint32 x, uint32 y, bool useMax) const
    {
        uint32 centerX = x, centerY = y;
        _heightmap.cellCenter(level, centerX, centerY);
        return _transform*Vec3f(float(centerX), _heightmap.at(level, x, y)[useMax], float(centerY));
    }

    bool splitRequired(uint32 level, uint32 x, uint32 y) const
    {
        //return level < 11;
        if (level == _heightmap.levels())
            return false;

        Vec3f up    = _camera.transform().up();
        Vec3f lower = globalCellPos(level, x, y, false);
        Vec3f upper = globalCellPos(level, x, y, true);
        Vec3f fwd   = lower - _camera.pos();
        Vec3f right = up.cross(fwd).normalized();

        Vec2f projL(right.dot(lower), up.dot(lower));
        Vec2f projU(right.dot(upper), up.dot(upper));
        float screenError = ((projU - projL)*Vec2f(_camera.resolution())).length()/fwd.length();

        return screenError > _maxError;
    }

    void buildHierarchy(TreeNode &node)
    {
        uint32 level = node.level();
        uint32 x = node.x();
        uint32 y = node.y();

        if (!splitRequired(level, x, y)) {
            _leafCount++;
            return;
        }

        node.split();

        buildHierarchy(node[0]);
        buildHierarchy(node[1]);
        buildHierarchy(node[2]);
        buildHierarchy(node[3]);
    }

    uint32 lookupDepth(const TreeNode &node, uint32 x, uint32 y) const
    {
        if (node.isLeaf()) {
            /*uint32 x0 = node.x() + 0, y0 = node.y() + 0;
            uint32 x1 = node.x() + 1, y1 = node.y() + 1;
            _heightmap.cellPos(node.level(), x0, y0);
            _heightmap.cellPos(node.level(), x1, y1);

            ASSERT(x >= x0 && y >= y0 && x < x1 && y < y1, "! (%d, %d) <= (%d, %d) < (%d, %d)", x0, y0, x, y, x1, y1);*/
            return node.level();
        } else {
            uint32 cx = node.x(), cy = node.y();
            _heightmap.cellCenter(node.level(), cx, cy);

            uint32 c = (x < cx ? 0 : 1) + (y < cy ? 0 : 2);
            return lookupDepth(node[c], x, y);
        }
    }

    uint32 buildVertex(std::unordered_map<uint64, uint32> &vertCache,
                       std::vector<Vertex> &verts,
                       uint32 level,
                       uint32 x,
                       uint32 y) const
    {
        _heightmap.cellPos(level, x, y);

        uint64 key = (uint64(x) << 32ULL) | uint64(y);
        auto iter = vertCache.find(key);

        if (iter == vertCache.end()) {
            verts.emplace_back(
                globalPos(x, y),
                Vec3f(0.0f),
                Vec2f(float(x)/float(_heightmap.width()),
                      float(y)/float(_heightmap.height()))
            );

            uint32 index = verts.size() - 1;
            vertCache.insert(std::make_pair(key, index));

            return index;
        } else
            return iter->second;
    }

    bool isTreeDeeper(uint32 level, uint32 x, uint32 y, int32 dx, int32 dy) const
    {
        uint32 ex = x, ey = y;
        _heightmap.cellPos(level, ex, ey);

        if ((dx < 0 && ex == 0) || (dx > 0 && ex == _heightmap.width() - 1))
            return false;
        if ((dy < 0 && ey == 0) || (dy > 0 && ey == _heightmap.height() - 1))
            return false;

        return lookupDepth(_root, ex + dx, ey + dy) > level;
    }

    uint32 buildSupportEdge(std::unordered_map<uint64, uint32> &vertCache,
                          std::vector<Vertex> &verts,
                          std::vector<TriangleI> &tris,
                          uint32 level,
                          uint32 x0,
                          uint32 y0,
                          uint32 x1,
                          uint32 y1,
                          uint32 dx,
                          uint32 dy,
                          uint32 first,
                          uint32 center) const
    {
        if (isTreeDeeper(level, x0, y0, dx, dy)) {
            x0 *= 2;
            x1 *= 2;
            y0 *= 2;
            y1 *= 2;
            uint32 cx = (x0 + x1)/2;
            uint32 cy = (y0 + y1)/2;

            uint32 last = buildSupportEdge(vertCache, verts, tris, level + 1, x0, y0, cx, cy, dx, dy, first, center);
            uint32 idx  = buildVertex(vertCache, verts, level + 1, cx, cy);
            tris.emplace_back(last, idx, center, 0);

            return buildSupportEdge(vertCache, verts, tris, level + 1, cx, cy, x1, y1, dx, dy, idx, center);
        } else
            return first;
    }

    bool isSupportQuad(const TreeNode &node) const
    {
        return
            isTreeDeeper(node.level(), node.x() + 0, node.y() + 0, -1,  0) ||
            isTreeDeeper(node.level(), node.x() + 0, node.y() + 0,  0, -1) ||
            isTreeDeeper(node.level(), node.x() + 1, node.y() + 1,  0, -1) ||
            isTreeDeeper(node.level(), node.x() + 1, node.y() + 1, -1,  0);
    }

    void buildSupportQuad(const TreeNode &node,
                          std::unordered_map<uint64, uint32> &vertCache,
                          std::vector<Vertex> &verts,
                          std::vector<TriangleI> &tris) const
    {
        uint32 center = buildVertex(vertCache, verts, node.level() + 1, node.x()*2 + 1, node.y()*2 + 1);

        uint32 idx00 = buildVertex(vertCache, verts, node.level(), node.x() + 0, node.y() + 0);
        uint32 idx10 = buildVertex(vertCache, verts, node.level(), node.x() + 1, node.y() + 0);
        uint32 idx11 = buildVertex(vertCache, verts, node.level(), node.x() + 1, node.y() + 1);
        uint32 idx01 = buildVertex(vertCache, verts, node.level(), node.x() + 0, node.y() + 1);

        uint32 support00 = buildSupportEdge(vertCache, verts, tris, node.level(), node.x() + 0, node.y() + 0, node.x() + 1, node.y() + 0,  0, -1, idx00, center);
        uint32 support10 = buildSupportEdge(vertCache, verts, tris, node.level(), node.x() + 1, node.y() + 0, node.x() + 1, node.y() + 1,  0,  0, idx10, center);
        uint32 support11 = buildSupportEdge(vertCache, verts, tris, node.level(), node.x() + 1, node.y() + 1, node.x() + 0, node.y() + 1, -1,  0, idx11, center);
        uint32 support01 = buildSupportEdge(vertCache, verts, tris, node.level(), node.x() + 0, node.y() + 1, node.x() + 0, node.y() + 0, -1, -1, idx01, center);

        tris.emplace_back(support00, idx10, center, 0);
        tris.emplace_back(support10, idx11, center, 0);
        tris.emplace_back(support11, idx01, center, 0);
        tris.emplace_back(support01, idx00, center, 0);
    }

    void buildRegularQuad(const TreeNode &node,
                          std::unordered_map<uint64, uint32> &vertCache,
                          std::vector<Vertex> &verts,
                          std::vector<TriangleI> &tris) const
    {
        uint32 idx0 = buildVertex(vertCache, verts, node.level(), node.x() + 0, node.y() + 0);
        uint32 idx1 = buildVertex(vertCache, verts, node.level(), node.x() + 1, node.y() + 0);
        uint32 idx2 = buildVertex(vertCache, verts, node.level(), node.x() + 1, node.y() + 1);
        uint32 idx3 = buildVertex(vertCache, verts, node.level(), node.x() + 0, node.y() + 1);

        tris.emplace_back(idx0, idx1, idx2, 0);
        tris.emplace_back(idx0, idx2, idx3, 0);
    }

    void buildMeshRecursive(const TreeNode &node,
                            std::unordered_map<uint64, uint32> &vertCache,
                            std::vector<Vertex> &verts,
                            std::vector<TriangleI> &tris) const
    {
        if (node.isLeaf()) {
            if (isSupportQuad(node))
                buildSupportQuad(node, vertCache, verts, tris);
            else
                buildRegularQuad(node, vertCache, verts, tris);
        } else {
            buildMeshRecursive(node[0], vertCache, verts, tris);
            buildMeshRecursive(node[1], vertCache, verts, tris);
            buildMeshRecursive(node[2], vertCache, verts, tris);
            buildMeshRecursive(node[3], vertCache, verts, tris);
        }
    }

public:
    HeightmapTerrain(const Camera &camera, const MinMaxChain &heightmap, const Mat4f &tform, float maxError)
    : _transform(tform), _maxError(maxError), _camera(camera), _heightmap(heightmap)
    {
        _leafCount = 0;
        buildHierarchy(_root);

        std::cout << _leafCount << " leaves" << std::endl;
    }

    std::shared_ptr<TriangleMesh> buildMesh(const std::shared_ptr<Material> &material, const std::string &name) const
    {
        std::unordered_map<uint64, uint32> vertCache;
        std::vector<Vertex> verts;
        std::vector<TriangleI> tris;
        buildMeshRecursive(_root, vertCache, verts, tris);

        return std::shared_ptr<TriangleMesh>(new TriangleMesh(std::move(verts), std::move(tris), material, name, true));
    }
};

}


#endif /* HEIGHTMAPTERRAIN_HPP_ */

