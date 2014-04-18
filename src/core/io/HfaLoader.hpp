#ifndef HFALOADER_HPP_
#define HFALOADER_HPP_

#include <fstream>

#include "extern/lodepng/lodepng.h"

#include "IntTypes.hpp"
#include "Debug.hpp"

#include "math/MathUtil.hpp"

namespace Tungsten
{

// Loads the clusterfuck that is the ERDAS IMAGINE HFA file format

class HfaLoader
{
    struct UnalignedUint32
    {
        uint16 l;
        uint16 h;

        UnalignedUint32() = default;
        UnalignedUint32(uint32 v)
        : l(v & 0xFFFF), h(v >> 16)
        {
        }

        operator uint32() const
        {
            return (uint32(h) << 16) | uint32(l);
        }
    };

    struct HeaderTag
    {
        char marker[16];
        uint32 fileHeader;
    };

    struct FileHeader
    {
        uint32 version;
        uint32 freeList;
        uint32 rootNode;
        uint16 entrySize;
        UnalignedUint32 dictionary;
    };

    struct TreeEntry
    {
        uint32 next;
        uint32 prev;
        uint32 parent;
        uint32 child;
        uint32 data;
        uint32 dataSize;
        char name[64];
        char type[32];
        uint32 modTime;
    };

    struct Layer
    {
        uint32 width;
        uint32 height;
        uint16 layerType;
        uint16 pixelType;
        uint32 blockWidth;
        uint32 blockHeight;
    };

    struct DataState
    {
        uint32 blockCount;
        uint32 pixelsPerBlock;
        uint32 unused;
        uint16 compressionType;
        UnalignedUint32 count;
        UnalignedUint32 firstBlock;
    };

    struct VirtualBlock
    {
        uint16 fileCode;
        UnalignedUint32 offset;
        UnalignedUint32 size;
        uint16 valid;
        uint16 compressionType;
    };

    struct CompressionHeader
    {
        uint32 minimum;
        uint32 rleSegments;
        uint32 pixelOffset;
        uint8 bpp;
    };

    struct MapInfo
    {
        uint32 c0; uint32 p0;
        double upperLeftX;
        double upperLeftY;
        uint32 c1; uint32 p1;
        double lowerRightX;
        double lowerRightY;
        uint32 c2; uint32 p2;
        double pixelSizeX;
        double pixelSizeY;
    };

    uint32 _w;
    uint32 _h;
    uint32 _blockW;
    uint32 _blockH;
    uint32 _blockIndex;
    uint32 _blocksPerW;
    float _minData;
    float _maxData;
    float _xScale;
    float _yScale;

    std::unique_ptr<uint8[]> _file;
    std::unique_ptr<uint32[]> _decompressedTile;
    std::unique_ptr<float[]> _tile;
    std::unique_ptr<float[]> _mapData;

    uint32 rleLength(uint32 &offset) {
        uint8 msb = _file[offset++];
        uint32 result = msb & 0x3Fu;
        uint32 count  = msb >> 6;
        for (uint32 i = 0; i < count; ++i)
            result = (result << 8) | _file[offset++];
        return result;
    }

    uint8 byteSwap(uint8 v)
    {
        return v;
    }

    uint16 byteSwap(uint16 v)
    {
        return (v >> 8) | (v << 8);
    }

    uint32 byteSwap(uint32 v)
    {
        return __builtin_bswap32(v);
    }

    template<typename T>
    void rleDecode(const CompressionHeader &header, uint32 countOffset)
    {
        uint32 dstIndex = 0;
        for (uint32 i = 0; i < header.rleSegments; ++i) {
            uint32 length = rleLength(countOffset);
            uint32 value = header.minimum + byteSwap(get<T>(header.pixelOffset + i*sizeof(T)));
            for (uint32 t = 0; t < length; ++t)
                _decompressedTile[dstIndex++] = value;
        }
    }

    void decompress(uint32 offset, uint32 size)
    {
        CompressionHeader header(get<CompressionHeader>(offset));
        header.pixelOffset += offset;

        switch(header.bpp) {
        case  8: rleDecode<uint8> (header, offset + 13); break;
        case 16: rleDecode<uint16>(header, offset + 13); break;
        case 32: rleDecode<uint32>(header, offset + 13); break;
        default: FAIL("Unsupported bpp: %d!", header.bpp);
        }
        memcpy(_tile.get(), _decompressedTile.get(), sizeof(uint32)*_blockW*_blockH);
    }

    template<typename T>
    T get(uint32 at)
    {
        T value;
        memcpy(&value, &_file[at], sizeof(T));
        return value;
    }

    void processBlock(const VirtualBlock &block)
    {
        uint32 startX = (_blockIndex % _blocksPerW)*_blockW;
        uint32 startY = (_blockIndex / _blocksPerW)*_blockH;

        if (block.compressionType)
            decompress(block.offset, block.size);
        else
            memcpy(_tile.get(), &_file[block.offset], _blockW*_blockH*sizeof(float));

        for (uint32 y = startY; y < min(startY + _blockH, _h); ++y)
            for (uint32 x = startX; x < min(startX + _blockW, _w); ++x)
                _mapData[x + y*_w] = _tile[(x - startX) + (y - startY)*_blockW];
    }

    void processState(const DataState &state)
    {
        _blockIndex = 0;

        uint32 count = state.count;
        uint32 offs = state.firstBlock;
        for (uint32 i = 0; i < count; ++i) {
            processBlock(get<VirtualBlock>(offs + i*sizeof(VirtualBlock)));
            _blockIndex++;
        }
    }

    void processLayer(const Layer &layer)
    {
        ASSERT(layer.pixelType == 9, "Only float pixel type supported!");

        _w = layer.width;
        _h = layer.height;
        _blockW = layer.blockWidth;
        _blockH = layer.blockHeight;
        _blocksPerW = (_w - 1)/_blockW + 1;

        _decompressedTile = std::unique_ptr<uint32[]>(new uint32[_blockW*_blockH]);
        _tile             = std::unique_ptr<float []>(new float [_blockW*_blockH]);
        _mapData          = std::unique_ptr<float []>(new float [_w*_h]);
    }

    void processInfo(const MapInfo &info)
    {
        float lrx = Angle::degToRad(info.lowerRightX);
        float lry = Angle::degToRad(info.lowerRightY);
        float ulx = Angle::degToRad(info.upperLeftX);
        float uly = Angle::degToRad(info.upperLeftY);

        _xScale = 1e3f/MathUtil::sphericalDistance(lry, lrx, lry, ulx, 6371.0f);
        _yScale = 1e3f/MathUtil::sphericalDistance(lry, lrx, uly, lrx, 6371.0f);
    }

    void processEntry(const TreeEntry &entry)
    {
        if (!strcmp(entry.type, "Eimg_Layer")) {
            processLayer(get<Layer>(entry.data));
        } else if (!strcmp(entry.type, "Edms_State")) {
            processState(get<DataState>(entry.data));
        } else if (!strcmp(entry.type, "Eprj_MapInfo")) {
            processInfo(get<MapInfo>(entry.data + 8 + get<uint32>(entry.data)));
        }
    }

    void traverse(uint32 idx)
    {
        do {
            TreeEntry entry(get<TreeEntry>(idx));
            processEntry(entry);

            if (entry.child)
                traverse(entry.child);

            idx = entry.next;
        } while (idx);
    }

public:
    HfaLoader(const std::string &src, const std::string &/*dst*/)
    {
        std::ifstream  in(src, std::ios_base::in  | std::ios_base::binary);
        //std::ofstream out(dst, std::ios_base::out | std::ios_base::binary);

        if (in.good()) {// && out.good()) {
            in.seekg(0, std::ifstream::end);
            size_t size = in.tellg();
            in.seekg(0, std::ifstream::beg);

            _file = std::unique_ptr<uint8[]>(new uint8[size]);
            in.read(reinterpret_cast<char *>(_file.get()), size);
            in.close();

            traverse(get<FileHeader>(get<HeaderTag>(0).fileHeader).rootNode);

            std::cout << _xScale << " " << _yScale << std::endl;

            /*uint32 w = _w;
            uint32 h = _h;
            std::unique_ptr<Vec4c[]> imgData(new Vec4c[w*h*4]);

            _minData =  1e30f;
            _maxData = -1e30f;
            for (uint32 y = 0; y < min(h, _h); ++y) {
                for (uint32 x = 0; x < min(w, _w); ++x) {
                    _minData = min(_minData, _mapData[x + y*_w]);
                    _maxData = max(_maxData, _mapData[x + y*_w]);
                }
            }
            for (uint32 y = 0; y < min(h, _h); ++y) {
                for (uint32 x = 0; x < min(w, _w); ++x) {
                    uint8 c = uint8((_mapData[x + y*_w] - _minData)/(_maxData - _minData)*255.0f);
                    imgData[x + y*w] = Vec4c(c, c, c, uint8(0xFF));
                }
            }

            out.close();
            lodepng_encode32_file(dst.c_str(), reinterpret_cast<uint8 *>(imgData.get()), w, h);*/
        }
    }

    uint32 w() const
    {
        return _w;
    }

    uint32 h() const
    {
        return _h;
    }

    float xScale() const
    {
        return _xScale;
    }

    float yScale() const
    {
        return _yScale;
    }

    std::unique_ptr<float[]> mapData()
    {
        return std::move(_mapData);
    }
};

}

#endif /* HFALOADER_HPP_ */
