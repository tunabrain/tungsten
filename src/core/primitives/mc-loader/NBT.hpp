#ifndef NBT_HPP_
#define NBT_HPP_

#include "io/FileUtils.hpp"

#include "IntTypes.hpp"
#include "Debug.hpp"

#include <cstring>

namespace Tungsten {
namespace MinecraftLoader {

class NbtTag
{
    enum NbtTagType {
        TAG_END        = 0,
        TAG_BYTE       = 1,
        TAG_SHORT      = 2,
        TAG_INT        = 3,
        TAG_LONG       = 4,
        TAG_FLOAT      = 5,
        TAG_DOUBLE     = 6,
        TAG_BYTE_ARRAY = 7,
        TAG_STRING     = 8,
        TAG_LIST       = 9,
        TAG_COMPOUND   = 10,
        TAG_INT_ARRAY  = 11,
        TAG_COUNT,
        TAG_INVALID    = 255,
    };

    struct Container {
        void *data;
        int32 length;
    };

    union Data {
        int8 b;
        int16 s;
        int32 i;
        int64 l;
        float f;
        double d;
        Container c;
    };

    static NbtTag InvalidTag;

    std::string _name;
    Data _data;
    NbtTagType _type;

    NbtTag()
    : _type(TAG_END)
    {
        std::memset(&_data, 0, sizeof(Data));
    }

    NbtTag(std::istream &s, NbtTagType type)
    : _type(type)
    {
        std::memset(&_data, 0, sizeof(Data));

        loadPayload(s);
    }

    template<typename T>
    void readBigEndian(std::istream &s, T &v)
    {
        uint8 *dst = reinterpret_cast<uint8 *>(&v);
        for (int i = sizeof(v) - 1; i >= 0; --i)
            dst[i] = s.get();
    }

    template<typename SizeType, typename ElemType>
    void loadArray(std::istream &s)
    {
        SizeType size;
        readBigEndian(s, size);
        _data.c.length = size;
        if (size > 0) {
            _data.c.data = new ElemType[size];
            if (sizeof(ElemType) == 1)
                s.read(reinterpret_cast<char *>(_data.c.data), size*sizeof(ElemType));
            else
                for (SizeType i = 0; i < size; ++i)
                    readBigEndian(s, as<ElemType>()[i]);
        } else {
            _data.c.data = nullptr;
        }
    }

    void loadPayload(std::istream &s)
    {
        switch (_type) {
        case TAG_BYTE:   _data.b = s.get(); break;
        case TAG_SHORT:  readBigEndian(s, _data.s); break;
        case TAG_INT:    readBigEndian(s, _data.i); break;
        case TAG_LONG:   readBigEndian(s, _data.l); break;
        case TAG_FLOAT:  readBigEndian(s, _data.f); break;
        case TAG_DOUBLE: readBigEndian(s, _data.d); break;
        case TAG_BYTE_ARRAY: loadArray<int32, int8>(s); break;
        case TAG_INT_ARRAY: loadArray<int32, int32>(s); break;
        case TAG_STRING: loadArray<uint16, int8>(s); break;
        case TAG_LIST: {
            int8 byteTag = s.get();
            if (byteTag < 0 || byteTag >= TAG_COUNT)
                FAIL("Invalid list tag type: %d", byteTag);
            NbtTagType type = NbtTagType(byteTag);

            readBigEndian(s, _data.c.length);
            if (_data.c.length > 0) {
                NbtTag *data = new NbtTag[_data.c.length];
                for (int i = 0; i < _data.c.length; ++i)
                    data[i] = NbtTag(s, type);
                _data.c.data = data;
            } else {
                _data.c.data = nullptr;
            }
            break;
        } case TAG_COMPOUND: {
            std::vector<NbtTag> tags;
            do {
                tags.emplace_back(s);
            } while (tags.back()._type != TAG_END);
            tags.pop_back();

            _data.c.length = int(tags.size());
            if (_data.c.length > 0) {
                _data.c.data = new NbtTag[tags.size()];
                for (int i = 0; i < _data.c.length; ++i)
                    as<NbtTag>()[i] = std::move(tags[i]);
            } else {
                _data.c.data = nullptr;
            }
            break;
        }
        default:
            break;
        }
    }

public:
    ~NbtTag()
    {
        switch (_type) {
        case TAG_BYTE_ARRAY: if (_data.c.data) delete[] as<int8>(); break;
        case TAG_STRING:     if (_data.c.data) delete[] as<int8>(); break;
        case TAG_LIST:       if (_data.c.data) delete[] as<NbtTag>(); break;
        case TAG_COMPOUND:   if (_data.c.data) delete[] as<NbtTag>(); break;
        case TAG_INT_ARRAY:  if (_data.c.data) delete[] as<int32>(); break;
        default: break;
        }
    }

    NbtTag(const NbtTag &c) = delete;

    NbtTag(NbtTag &&o)
    {
        _name = std::move(o._name);
        _type = o._type;
        std::memcpy(&_data, &o._data, sizeof(Data));
        o._data.c.data = nullptr;
    }

    void operator=(NbtTag &&o)
    {
        _name = std::move(o._name);
        _type = o._type;
        std::memcpy(&_data, &o._data, sizeof(Data));
        o._data.c.data = nullptr;
    }

    NbtTag(std::istream &s)
    {
        std::memset(&_data, 0, sizeof(Data));

        int8 type;
        FileUtils::streamRead(s, type);
        if (type < 0 || type >= TAG_COUNT)
            FAIL("Invalid tag type: %d", type);

        _type = NbtTagType(type);
        if (_type == TAG_END)
            return;

        loadArray<uint16, uint8>(s);
        if (_data.c.data) {
            _name = std::string(as<const char>(), size_t(_data.c.length));
            delete[] as<uint8>();
            _data.c.data = nullptr;
            _data.c.length = 0;
        }

        loadPayload(s);
    }

    template<typename T>
    T *as()
    {
        return reinterpret_cast<T *>(_data.c.data);
    }

    template<typename T>
    const T *as() const
    {
        return reinterpret_cast<const T *>(_data.c.data);
    }

    NbtTag &operator[](const char *name)
    {
        if (_type != TAG_LIST && _type != TAG_COMPOUND)
            return InvalidTag;

        for (int i = 0; i < _data.c.length; ++i)
            if (as<NbtTag>()[i]._name == name)
                return as<NbtTag>()[i];

        return InvalidTag;
    }

    int operator[](int idx) const
    {
        if (!_data.c.data)
            return 0;

        if (_type == TAG_BYTE_ARRAY)
            return as<int8>()[idx];
        if (_type == TAG_INT_ARRAY)
            return as<int32>()[idx];

        return 0;
    }

    operator bool() const
    {
        return _type != TAG_INVALID;
    }

    NbtTag &subtag(int idx)
    {
        if (_type != TAG_LIST && _type != TAG_COMPOUND)
            return InvalidTag;
        else
            return as<NbtTag>()[idx];
    }

    int size() const
    {
        switch (_type) {
        case TAG_BYTE_ARRAY:
        case TAG_STRING:
        case TAG_LIST:
        case TAG_COMPOUND:
        case TAG_INT_ARRAY:
            return _data.c.length;
        default:
            return 0;
        }
    }

    int asInt() const
    {
        switch (_type) {
        case TAG_BYTE:  return _data.b;
        case TAG_SHORT: return _data.s;
        case TAG_INT:   return _data.i;
        default: return 0;
        }
    }

    int64 asLong() const
    {
        switch (_type) {
        case TAG_BYTE:  return _data.b;
        case TAG_SHORT: return _data.s;
        case TAG_INT:   return _data.i;
        case TAG_LONG:  return _data.l;
        default: return 0;
        }
    }

    float asFloat() const
    {
        if (_type == TAG_FLOAT)
            return _data.f;
        else
            return 0.0f;
    }

    double asDouble() const
    {
        if (_type == TAG_DOUBLE)
            return _data.d;
        else if (_type == TAG_FLOAT)
            return _data.f;
        else
            return 0.0f;
    }
};

}
}

#endif /* NBT_HPP_ */
