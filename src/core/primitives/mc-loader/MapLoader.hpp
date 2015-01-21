#ifndef MAPLOADER_HPP_
#define MAPLOADER_HPP_

#include "ResourcePackLoader.hpp"
#include "MemBuf.hpp"
#include "File.hpp"
#include "NBT.hpp"

#include <miniz/miniz.h>
#include <iostream>
#include <memory>
#include <cstdio>

namespace Tungsten {

class MapLoader
{
    static const size_t CompressedChunkSize = 1024*1024;
    static const size_t DecompressedChunkSize = 5*1024*1024;

    std::string _path;

    std::unique_ptr<uint8[]> _locationTable, _timestampTable;
    std::unique_ptr<uint8[]> _compressedChunk, _decompressedChunk;
    std::unique_ptr<uint16[]> _regionGrid;
    int _regionHeight;

    void loadChunk(std::istream &in, int chunkX, int chunkZ)
    {
        NbtTag root(in);

        NbtTag &sections = root["Level"]["Sections"];
        for (int i = 0; i < sections.size(); ++i) {
            NbtTag &blocks = sections.subtag(i)["Blocks"];
            NbtTag &add    = sections.subtag(i)["Add"];
            NbtTag &data   = sections.subtag(i)["Data"];
            int chunkY = sections.subtag(i)["Y"].asInt();

            int gridOffset = (chunkX/16) + 2*(chunkZ/16);
            int base = gridOffset*256*256*256 + 16*((chunkX % 16) + 256*chunkY + 256*256*(chunkZ % 16));

            for (int z = 0; z < 16; ++z) {
                for (int y = 0; y < 16; ++y) {
                    for (int x = 0; x < 16; ++x) {
                        int idx = x + z*16 + y*16*16;

                        uint16 blockId = 0;
                        if (blocks) blockId |= blocks[idx] << 4;
                        if (add   ) blockId |= ((add [idx/2] >> ((idx & 1)*4)) & 0xF) << 12;
                        if (data  ) blockId |= ((data[idx/2] >> ((idx & 1)*4)) & 0xF);

                        _regionGrid[base + x + 256*y + 256*256*z] = blockId;

                        if (blockId)
                            _regionHeight = max(_regionHeight, chunkY*16 + y);
                    }
                }
            }
        }
    }

    void loadRegion(std::istream &in)
    {
        FileUtils::streamRead(in, _locationTable.get(), 4096);
        FileUtils::streamRead(in, _timestampTable.get(), 4096);

        std::memset(_regionGrid.get(), 0, 512*256*512*sizeof(uint16));
        _regionHeight = 0;

        for (int i = 0; i < 1024; ++i) {
            uint32 offset = 4*1024*(
                (uint32(_locationTable[i*4 + 0]) << 16) +
                (uint32(_locationTable[i*4 + 1]) <<  8) +
                 uint32(_locationTable[i*4 + 2]));
            uint32 length = uint32(_locationTable[i*4 + 3])*4*1024;

            if (offset == 0 || length == 0)
                continue;

            in.seekg(offset);
            FileUtils::streamRead(in, _compressedChunk.get(), length);

            uint32 chunkLength =
                (uint32(_compressedChunk[0]) << 24) +
                (uint32(_compressedChunk[1]) << 16) +
                (uint32(_compressedChunk[2]) <<  8) +
                 uint32(_compressedChunk[3]);
            if (_compressedChunk[4] != 2) // Only accept Zlib compression
                continue;

            uLongf destLength = DecompressedChunkSize;
            if (uncompress(_decompressedChunk.get(), &destLength, _compressedChunk.get() + 5, chunkLength) != Z_OK)
                continue;

            MemBuf buffer(reinterpret_cast<char *>(_decompressedChunk.get()), destLength);
            std::istream bufferStream(&buffer);

            loadChunk(bufferStream, i % 32, i/32);
        }
    }

public:
    MapLoader(const std::string &path)
    : _path(path),
      _regionHeight(0)
    {
        _locationTable.reset(new uint8[4096]);
        _timestampTable.reset(new uint8[4096]);
        _compressedChunk.reset(new uint8[CompressedChunkSize]);
        _decompressedChunk.reset(new uint8[DecompressedChunkSize]);
        _regionGrid.reset(new uint16[512*256*512]);
    }

    void loadRegions(const std::function<void(int, int, int, uint16 *)> &regionHandler) {
        File dir(_path);
        if (!dir.exists() || !dir.isDirectory())
            return;
        File region(dir.file("region"));
        if (!region.exists() || !region.isDirectory())
            return;

        for (File iter = region.beginScan(File::fileFilter()); iter.valid(); iter = region.scan()) {
            if (!FileUtils::testExtension(iter.path(), "mca"))
                continue;

            std::string base = iter.baseName();
            if (base.length() < 2 || tolower(base.front()) != 'r' || base[1] != '.')
                continue;
            int x, z;
            if (std::sscanf(base.c_str() + 2, "%i.%i", &x, &z) != 2)
                continue;

            std::ifstream in(iter.path(), std::ios_base::in | std::ios_base::binary);
            if (!in.good())
                continue;
            loadRegion(in);

            regionHandler(x*2 + 0, z*2 + 0, _regionHeight, _regionGrid.get());
            regionHandler(x*2 + 1, z*2 + 0, _regionHeight, _regionGrid.get() + 256*256*256);
            regionHandler(x*2 + 0, z*2 + 1, _regionHeight, _regionGrid.get() + 256*256*256*2);
            regionHandler(x*2 + 1, z*2 + 1, _regionHeight, _regionGrid.get() + 256*256*256*3);
        }
    }
};

}

#endif /* MAPLOADER_HPP_ */
