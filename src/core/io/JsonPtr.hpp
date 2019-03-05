#ifndef JSONPTR_HPP_
#define JSONPTR_HPP_

#include "math/Mat4f.hpp"
#include "math/Vec.hpp"

#include "Platform.hpp"

#include <rapidjson/document.h>
#include <tinyformat/tinyformat.hpp>

namespace Tungsten {

class JsonMemberIterator;
class JsonDocument;
class Path;

class JsonPtr
{
    friend JsonMemberIterator;
    friend JsonDocument;

protected:
    const JsonDocument *_document;
    const rapidjson::Value *_value;

    JsonPtr(const JsonDocument *document, const rapidjson::Value *value)
    : _document(document),
      _value(value)
    {
    }

public:
    JsonPtr()
    : JsonPtr(nullptr, nullptr)
    {
    }

    void get(bool &dst) const;
    void get(float &dst) const;
    void get(double &dst) const;
    void get(uint8 &dst) const;
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

    JsonPtr operator[](unsigned i) const
    {
        if (!_value->IsArray())
            return JsonPtr();

        return JsonPtr(_document, &(*_value)[i]);
    }

    JsonPtr operator[](const char *field) const
    {
        if (!_value->IsObject())
            return JsonPtr();

        auto member = _value->FindMember(field);
        if (member == _value->MemberEnd())
            return JsonPtr();

        return JsonPtr(_document, &member->value);
    }

    JsonPtr getRequiredMember(const char *field) const
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

    NORETURN void parseError(std::string description) const;

    operator bool() const { return _value != nullptr; }

    bool isObject() const { return _value && _value->IsObject(); }
    bool isArray () const { return _value && _value->IsArray (); }
    bool isString() const { return _value && _value->IsString(); }
    bool isNumber() const { return _value && _value->IsNumber(); }

    JsonMemberIterator begin() const;
    JsonMemberIterator   end() const;
};

class JsonMemberIterator
{
    friend JsonPtr;

    const JsonDocument *_document;
    const rapidjson::Value *_value;
    rapidjson::Value::ConstMemberIterator _iter;

    JsonMemberIterator(const JsonPtr &v, rapidjson::Value::ConstMemberIterator iter)
    : _document(v._document),
      _value(v._value),
      _iter(iter)
    {
    }

public:
    bool operator!=(const JsonMemberIterator &o)
    {
        return _iter != o._iter;
    }

    JsonMemberIterator operator++()
    {
        _iter++;
        return *this;
    }

    std::pair<const char *, JsonPtr> operator*() const
    {
        return std::make_pair(_iter->name.GetString(), JsonPtr(_document, &_iter->value));
    }
};

}

#endif /* JSONPTR_HPP_ */
