#ifndef JSONVALUE_HPP_
#define JSONVALUE_HPP_

#include "math/Mat4f.hpp"
#include "math/Vec.hpp"

#include <rapidjson/document.h>

namespace Tungsten {

class JsonValueIterator;
class JsonDocument;
class Path;

class JsonValue
{
    friend JsonValueIterator;

    const JsonDocument *_document;
    const rapidjson::Value *_value;

public:
    JsonValue()
    : JsonValue(nullptr, nullptr)
    {
    }

    JsonValue(const JsonDocument *document, const rapidjson::Value *value)
    : _document(document),
      _value(value)
    {
    }

    void get(bool &dst) const;
    void get(float &dst) const;
    void get(double &dst) const;
    void get(uint32 &dst) const;
    void get(int32 &dst) const;
    void get(uint64 &dst) const;
    void get(int64 &dst) const;
    void get(std::string &dst) const;
    void get(Mat4f &dst) const;
    void get(Path &dst) const;

    template<typename ElementType, unsigned Size>
    void get(Vec<ElementType, Size> &dst) const
    {
        if (!_value->IsArray()) {
            dst = Vec<ElementType, Size>(cast<ElementType>());
        } else {
            //TODO error
            //ASSERT(_value->Size() == Size,
            //    "Cannot convert Json Array to vector: Invalid size. Expected %d, received %d", Size, _value->Size());

            for (unsigned i = 0; i < Size; ++i)
                dst[i] = (*this)[i].cast<ElementType>();
        }
    }

    template<typename T>
    T cast() const
    {
        T t;
        get(t);
        return t;
    }

    template<typename T>
    T castField(const char *field) const
    {
        return getRequiredMember(field).cast<T>();
    }

    template<typename T>
    bool getField(const char *field, T &dst) const
    {
        if (auto member = this->operator [](field)) {
            member.get(dst);
            return true;
        }
        return false;
    }

    JsonValue operator[](unsigned i) const
    {
        if (!_value->IsArray())
            return JsonValue();

        return JsonValue(_document, &(*_value)[i]);
    }

    JsonValue operator[](const char *field) const
    {
        if (!_value->IsObject())
            return JsonValue();

        auto member = _value->FindMember(field);
        if (member == _value->MemberEnd())
            return JsonValue();

        return JsonValue(_document, &member->value);
    }

    JsonValue getRequiredMember(const char *field) const
    {
        if (auto result = operator [](field))
            return result;
        // TODO error
    }

    unsigned size() const
    {
        if (!_value->IsArray())
            return 0;
        return _value->Size();
    }

    operator bool() const { return _value != nullptr; }

    bool isObject() const { return _value->IsObject(); }
    bool isArray () const { return _value->IsArray (); }
    bool isString() const { return _value->IsString(); }
    bool isNumber() const { return _value->IsNumber(); }

    JsonValueIterator begin() const;
    JsonValueIterator   end() const;
};

class JsonValueIterator
{
    friend JsonValue;

    const JsonDocument *_document;
    const rapidjson::Value *_value;
    rapidjson::Value::ConstMemberIterator _iter;

    JsonValueIterator(const JsonValue &v, rapidjson::Value::ConstMemberIterator iter)
    : _document(v._document),
      _value(v._value),
      _iter(iter)
    {
    }

public:
    bool operator!=(const JsonValueIterator &o)
    {
        return _iter != o._iter;
    }

    JsonValueIterator operator++()
    {
        _iter++;
        return *this;
    }

    std::pair<const char *, JsonValue> operator*() const
    {
        return std::make_pair(_iter->name.GetString(), JsonValue(_document, &_iter->value));
    }
};

}

#endif /* JSONVALUE_HPP_ */
