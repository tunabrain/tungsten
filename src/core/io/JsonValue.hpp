#ifndef JSONVALUE_HPP_
#define JSONVALUE_HPP_

#include "math/Mat4f.hpp"
#include "math/Vec.hpp"

#include <rapidjson/document.h>
#include <tinyformat/tinyformat.hpp>

namespace Tungsten {

class JsonValueIterator;
class JsonDocument;
class Path;

class JsonValue
{
    friend JsonValueIterator;
    friend JsonDocument;

protected:
    const JsonDocument *_document;
    const rapidjson::Value *_value;

    JsonValue(const JsonDocument *document, const rapidjson::Value *value)
    : _document(document),
      _value(value)
    {
    }

public:
    JsonValue()
    : JsonValue(nullptr, nullptr)
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
    void get(const char *&dst) const;
    void get(Mat4f &dst) const;
    void get(Path &dst) const;

    template<typename ElementType, unsigned Size>
    void get(Vec<ElementType, Size> &dst) const
    {
        if (!_value->IsArray()) {
            dst = Vec<ElementType, Size>(cast<ElementType>());
        } else {
            if (size() != Size)
                parseError(tfm::format("Trying to parse a Vec%d, but this array has the wrong size "
                        "(need %d elements, received %d)", Size, Size, size()));

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
        if (!isObject())
            parseError("Type mismatch: Expecting a JSON object here");
        if (auto result = operator [](field))
            return result;
        parseError(tfm::format("Object is missing required field \"%s\"", field));
    }

    unsigned size() const
    {
        if (!_value->IsArray())
            return 0;
        return _value->Size();
    }

    [[noreturn]] void parseError(std::string description) const;

    operator bool() const { return _value != nullptr; }

    bool isObject() const { return _value && _value->IsObject(); }
    bool isArray () const { return _value && _value->IsArray (); }
    bool isString() const { return _value && _value->IsString(); }
    bool isNumber() const { return _value && _value->IsNumber(); }

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
