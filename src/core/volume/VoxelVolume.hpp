#ifndef VOXELVOLUME_HPP_
#define VOXELVOLUME_HPP_

#include "io/JsonSerializable.hpp"
#include "io/JsonUtils.hpp"
#include "io/FileUtils.hpp"

#include <type_traits>

namespace Tungsten {

template<bool Scalar>
class VoxelVolume : public JsonSerializable
{
protected:
    typedef typename std::conditional<Scalar, float, Vec3f>::type Value;

private:
    float lerp(float x000, float x010, float x100, float x110,
               float x001, float x011, float x101, float x111,
               float u, float v, float w) const
    {
        return ((x000*(1.0f - u) + x010*u)*(1.0f - v) + (x100*(1.0f - u) + x110*u)*v)*(1.0f - w) +
               ((x001*(1.0f - u) + x011*u)*(1.0f - v) + (x101*(1.0f - u) + x111*u)*v)*w;
    }

    Vec3f lerp(Vec3f x000, Vec3f x010, Vec3f x100, Vec3f x110,
               Vec3f x001, Vec3f x011, Vec3f x101, Vec3f x111,
               float u, float v, float w) const
    {
        return ((x000*(1.0f - u) + x010*u)*(1.0f - v) + (x100*(1.0f - u) + x110*u)*v)*(1.0f - w) +
               ((x001*(1.0f - u) + x011*u)*(1.0f - v) + (x101*(1.0f - u) + x111*u)*v)*w;
    }

protected:
    std::string _srcDir;
    std::string _path;

    int _w, _h, _d;
    Mat4f _transform;
    Mat4f _invTransform;
    Value _min, _max, _avg;

    virtual Value get(int x, int y, int z) const = 0;

public:
    VoxelVolume(const std::string &path)
    : _srcDir(FileUtils::getCurrentDir()),
      _path(path)
    {
    }

    ~VoxelVolume() {}

    virtual void fromJson(const rapidjson::Value &/*v*/, const Scene &/*scene*/) override final
    {
    }

    virtual rapidjson::Value toJson(Allocator &allocator) const override final
    {
        return std::move(rapidjson::Value(_path.c_str(), allocator));
    }

    void saveData() const
    {
        if (FileUtils::getCurrentDir() != _srcDir)
            FileUtils::copyFile(FileUtils::addSlash(_srcDir) + _path, _path, true);
    }

    std::string fullPath() const
    {
        return std::move(FileUtils::addSlash(_srcDir) + _path);
    }

    const std::string &path() const
    {
        return _path;
    }

    void setPath(const std::string &s)
    {
        _path = s;
    }

    Value operator[](Vec3f uv) const
    {
        uv = _invTransform*uv;
        float u = uv.x()*_w;
        float v = (1.0f - uv.y())*_h;
        float w = uv.z()*_d;
        int iu = int(u);
        int iv = int(v);
        int iw = int(w);
        u -= iu;
        v -= iv;
        w -= iw;
        iu = ((iu % _w) + _w) % _w;
        iv = ((iv % _h) + _h) % _h;
        iw = ((iw % _d) + _d) % _d;
        iu = clamp(iu, 0, _w - 2);
        iv = clamp(iv, 0, _h - 2);
        iw = clamp(iw, 0, _d - 2);

        return lerp(
            get(iu, iv, iw + 0), get(iu + 1, iv, iw + 0), get(iu, iv + 1, iw + 0), get(iu + 1, iv + 1, iw + 0),
            get(iu, iv, iw + 1), get(iu + 1, iv, iw + 1), get(iu, iv + 1, iw + 1), get(iu + 1, iv + 1, iw + 1),
            u, v, w
        );
    }

    template<typename LAMBDA>
    inline void dda(const Vec3f &posGlobal, const Vec3f &dirGlobal, float tMin, float tMax, LAMBDA cellFunc)
    {
        Vec3f size((float(_w)), float(_h), float(_d));
        Vec3f pos0((_invTransform*(posGlobal + dirGlobal*tMin))*size);
        Vec3f pos1((_invTransform*(posGlobal + dirGlobal*tMax))*size);
        Vec3f dir(pos1 - pos0);
        float maxT = dir.length();
        dir /= maxT;
        Vec3f invDir = std::abs(1.0f/dir);

        int cellX = int(pos0.x());
        int cellY = int(pos0.y());
        int cellZ = int(pos0.z());

        int stepX = dir.x() < 0.0f ? -1 : 1;
        int stepY = dir.y() < 0.0f ? -1 : 1;
        int stepZ = dir.z() < 0.0f ? -1 : 1;

        float minTx = dir.x() < 0.0f ? (pos0.x() - cellX)*invDir.x() : (cellX + 1 - pos0.x())*invDir.x();
        float minTy = dir.y() < 0.0f ? (pos0.y() - cellY)*invDir.y() : (cellY + 1 - pos0.y())*invDir.y();
        float minTz = dir.z() < 0.0f ? (pos0.z() - cellZ)*invDir.z() : (cellZ + 1 - pos0.z())*invDir.z();

        cellX = ((cellX % _w) + _w) % _w;
        cellY = ((cellY % _h) + _h) % _h;
        cellZ = ((cellZ % _d) + _d) % _d;

        float t = 0.0f;
        while (t < maxT) {
            Value cell(get(cellX, cellY, cellZ));
            float newT;
            if (minTx < minTy && minTx < minTz) {
                cellX += stepX;
                if (cellX < 0) cellX += _w; else if (cellX >= _w) cellX -= _w;
                newT = minTx;
                minTx += invDir.x();
            } else if (minTy < minTz) {
                cellY += stepY;
                if (cellY < 0) cellY += _h; else if (cellY >= _h) cellY -= _h;
                newT = minTy;
                minTy += invDir.y();
            } else {
                cellZ += stepZ;
                if (cellZ < 0) cellZ += _d; else if (cellZ >= _d) cellZ -= _d;
                newT = minTz;
                minTz += invDir.z();
            }

            if (cellFunc(cell, min(newT, maxT) - t))
                break;
            t = newT;
        }
    }

    void setTransform(const Mat4f &mat)
    {
        _transform = mat;
        _invTransform = mat.pseudoInvert();
    }
};

struct Rgb
{
    uint8 c[3];

    Rgb() = default;

    Rgb(uint8 t)
    : c{t, t, t}
    {
    }

    Rgb(uint8 r, uint8 g, uint8 b)
    : c{r, g, b}
    {
    }
};

template<bool Scalar, typename Texel>
class VoxelVolumeImpl : public VoxelVolume<Scalar>
{
    typedef typename VoxelVolume<Scalar>::Value Value;

    using VoxelVolume<Scalar>::_w;
    using VoxelVolume<Scalar>::_h;
    using VoxelVolume<Scalar>::_d;

    std::unique_ptr<Texel[]> _voxels;

    inline float convert(float f) const
    {
        return f;
    }

    inline float convert(uint8 c) const
    {
        return c*1.0f/255.0f;
    }

    inline Vec3f convert(const Vec3f &x) const
    {
        return x;
    }

    inline Vec3f convert(Rgb x) const
    {
        return Vec3f(x.c[0]*1.0f/255.0f, x.c[1]*1.0f/255.0f, x.c[2]*1.0f/255.0f);
    }

protected:
    virtual Value get(int x, int y, int z) const override final
    {
        return Value(convert(_voxels[(z*_h + y)*_w + x]));
    }

public:
    VoxelVolumeImpl(const std::string &path, std::unique_ptr<Texel[]> voxels,
            int w, int h, int d)
    : VoxelVolume<Scalar>(path),
      _voxels(std::move(voxels))
    {
        _w = w;
        _h = h;
        _d = d;
    }
};

namespace VoxelUtils
{

template<bool Scalar>
inline std::shared_ptr<VoxelVolume<Scalar>> loadVolume(const std::string &path)
{
    typedef typename std::conditional<Scalar, uint8, Rgb>::type LdrType;
    typedef typename std::conditional<Scalar, float, Vec3f>::type HdrType;

    std::ifstream in(path, std::ios_base::in | std::ios_base::binary);
    if (!in.good())
        return nullptr;
    char magic[3];
    in.read(magic, 3);
    if (magic[0] != 'V' || magic[1] != 'O' || magic[2] != 'L')
        return nullptr;
    uint8 version;
    uint32 encoding, w, h, d, channels;
    FileUtils::streamRead(in, version);
    FileUtils::streamRead(in, encoding);
    FileUtils::streamRead(in, w);
    FileUtils::streamRead(in, h);
    FileUtils::streamRead(in, d);
    FileUtils::streamRead(in, channels);

    if (version != 3)
        return nullptr;
    if (encoding != 1 && encoding != 3)
        return nullptr;
    if (channels != 1 && channels != 3)
        return nullptr;

    std::cout << tfm::format("VOL status report: Resolution %dx%dx%d, Channels %d, bytes-per-channel %d", w, h, d, channels, encoding == 1 ? 4 : 1) << std::endl;

    if (encoding == 1) {
        std::unique_ptr<float[]> texels(new float[w*h*d*channels]);
        in.read(reinterpret_cast<char *>(texels.get()), w*h*d*channels*sizeof(float));

        if (channels == 3 && Scalar) {
            std::unique_ptr<float[]> grayTexels(new float[w*h*d]);
            for (uint64 i = 0; i < w*h*d; ++i)
                grayTexels[i] = (texels[i*3] + texels[i*3 + 1] + texels[i*3 + 2])*(1.0f/3.0f);
            texels = std::move(grayTexels);
        }

        return std::make_shared<VoxelVolumeImpl<Scalar, HdrType>>(path,
            std::unique_ptr<HdrType[]>(reinterpret_cast<HdrType *>(texels.release())), w, h, d);
    } else {
        std::unique_ptr<uint8[]> texels(new uint8[w*h*d*channels]);
        in.read(reinterpret_cast<char *>(texels.get()), w*h*d*channels*sizeof(uint8));

        if (channels == 3 && Scalar) {
            std::unique_ptr<uint8[]> grayTexels(new uint8[w*h*d]);
            for (uint64 i = 0; i < w*h*d; ++i)
                grayTexels[i] = uint8((int(texels[i*3]) + int(texels[i*3 + 1]) + int(texels[i*3 + 2]))/3);
            texels = std::move(grayTexels);
        }

        return std::make_shared<VoxelVolumeImpl<Scalar, LdrType>>(path,
            std::unique_ptr<LdrType[]>(reinterpret_cast<LdrType *>(texels.release())), w, h, d);
    }
}

}

typedef VoxelVolume<true> VoxelVolumeA;
typedef VoxelVolume<false> VoxelVolumeRgb;

}

#endif /* VOXELVOLUME_HPP_ */
