#include "JsonDocument.hpp"

#include "JsonLoadException.hpp"
#include "JsonPtr.hpp"
#include "FileUtils.hpp"

#include "math/MathUtil.hpp"

#include "IntTypes.hpp"

#include <tinyformat/tinyformat.hpp>
#include <rapidjson/error/en.h>
#include <vector>
#include <cctype>
#include <stack>

#define JsonParseFlags rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag | rapidjson::kParseNanAndInfFlag

namespace Tungsten {

typedef rapidjson::Value Value;

struct Excerpt
{
    int row, col;
    std::string excerpt;
    std::string pointer;
};
struct TrackedValue
{
    size_t offset;
    TrackedValue *members;
    TrackedValue(size_t offset_) : offset(offset_), members(nullptr) {}
};

class OffsetTrackingDocument : public rapidjson::Document
{
    typedef rapidjson::GenericStringStream<rapidjson::UTF8< >> StringStream;
    typedef rapidjson::SizeType Size;
    typedef GenericDocument Document;

    std::stack<TrackedValue> _stack;
    StringStream _stream;

    void push()
    {
        _stack.emplace(_stream.Tell());
    }

    void pushCompound(int memberCount)
    {
        TrackedValue *members =
                static_cast<TrackedValue *>(GetAllocator().Malloc(memberCount*sizeof(TrackedValue)));
        for (int i = memberCount - 1; i >= 0; --i) {
            members[i] = _stack.top();
            _stack.pop();
        }
        push();
        _stack.top().members = members;
    }

public:
    OffsetTrackingDocument(const std::string &s) : _stream(s.c_str()) {}

    template<unsigned ParseFlags>
    void parse()
    {
        rapidjson::GenericReader<rapidjson::UTF8<char>, rapidjson::UTF8<char>> reader;
        reader.template Parse<ParseFlags>(_stream, *this);
    }

    bool Null()           { push(); return Document::Null();    }
    bool Bool(bool b)     { push(); return Document::Bool(b);   }
    bool Int(int32 i)     { push(); return Document::Int(i);    }
    bool Uint(uint32 i)   { push(); return Document::Uint(i);   }
    bool Int64(int64 i)   { push(); return Document::Int64(i);  }
    bool Uint64(uint64 i) { push(); return Document::Uint64(i); }
    bool Double(double d) { push(); return Document::Double(d); }

    bool RawNumber(const Ch *str, Size length, bool c) { push(); return Document::RawNumber(str, length, c); }
    bool String   (const Ch *str, Size length, bool c) { push(); return Document::String   (str, length, c); }
    bool Key      (const Ch *str, Size length, bool c) { push(); return Document::Key      (str, length, c); }

    bool StartObject() { return Document::StartObject(); }
    bool StartArray () { return Document::StartArray (); }

    bool EndObject(Size count) { pushCompound(count*2); return Document::EndObject(count); }
    bool EndArray (Size count) { pushCompound(count  ); return Document::EndArray (count); }

    const TrackedValue &root()
    {
        return _stack.top();
    }
};

Excerpt formatJsonExcerpt(const std::string &json, size_t offset)
{
    const int MaxLineLength = 90;
    const int CropLineLength = 80;

    std::vector<size_t> lineEnds;
    for (size_t i = 0; i < json.size(); ++i)
        if (json[i] == '\n')
            lineEnds.push_back(i + 1);
    lineEnds.push_back(json.size());

    Excerpt result{0, 0, "", ""};

    if (offset >= json.size())
        return result;

    for (size_t i = 0; i < lineEnds.size(); ++i) {
        if (offset < lineEnds[i]) {
            size_t lineStart = i == 0 ? 0 : lineEnds[i - 1];
            int lineLength = lineEnds[i] - lineStart;
            while (lineLength > 0 && std::iscntrl(json[lineStart + lineLength - 1]))
                lineLength--;

            int row = i;
            int col = offset - lineStart;

            int right, left;
            if (lineLength > MaxLineLength) {
                right = min(col + CropLineLength/2, lineLength);
                left  = max(right - CropLineLength, 0);
                right = min(left + CropLineLength, lineLength);

                col -= left;
            } else {
                left = 0;
                right = lineLength;
            }

            result.row = row;
            result.col = col;
            result.excerpt = json.substr(lineStart + left, right - left);

            if (left > 0) {
                result.excerpt = "..." + result.excerpt;
                col += 3;
            }
            if (right < lineLength)
                result.excerpt += "...";
            result.pointer = std::string(col, '-') + '^';
            break;
        }
    }
    return result;
}

const TrackedValue *findOffset(const Value *root, const TrackedValue *trackedRoot, const Value *target)
{
    if (root == target)
        return trackedRoot;
    if (root->IsObject()) {
        const TrackedValue *trackedMembers = trackedRoot->members;
        for (auto iter = root->MemberBegin(); iter != root->MemberEnd(); ++iter, trackedMembers += 2) {
            if (auto result = findOffset(&iter->name,  trackedMembers + 0, target)) return result;
            if (auto result = findOffset(&iter->value, trackedMembers + 1, target)) return result;
        }
    } else if (root->IsArray()) {
        for (rapidjson::SizeType i = 0; i < root->Size(); ++i)
            if (auto result = findOffset(&(*root)[i], trackedRoot->members + i, target))
                return result;
    }
    return nullptr;
}

void JsonDocument::load()
{
    _document.Parse<JsonParseFlags>(_json.c_str());
    if (_document.HasParseError()) {
        Excerpt excerpt = formatJsonExcerpt(_json, _document.GetErrorOffset());
        throw JsonLoadException(
            tfm::format("Encountered a syntax error at %s:%d:%d:\n    %s",
                    _file.fileName(), excerpt.row + 1, excerpt.col + 1, rapidjson::GetParseError_En(_document.GetParseError())),
            tfm::format("%s\n%s", excerpt.excerpt, excerpt.pointer)
        );
    }
}

JsonDocument::JsonDocument(const Path &file)
: JsonPtr(this, &_document),
  _file(file),
  _json(FileUtils::loadText(file))
{
    if (_json.empty())
        throw JsonLoadException(file);

    load();
}

JsonDocument::JsonDocument(const Path &file, std::string json)
: JsonPtr(this, &_document),
  _file(file),
  _json(std::move(json))
{
    load();
}

void JsonDocument::parseError(JsonPtr source, std::string description) const
{
    OffsetTrackingDocument trackedDocument(_json);
    trackedDocument.parse<JsonParseFlags>();

    const TrackedValue *value = findOffset(&_document, &trackedDocument.root(), source._value);
    if (value) {
        Excerpt excerpt = formatJsonExcerpt(_json, value->offset);
        throw JsonLoadException(
            tfm::format("Encountered an error at %s:%d:%d:\n    %s",
                    _file.fileName(), excerpt.row + 1, excerpt.col + 1, description),
            tfm::format("%s\n%s", excerpt.excerpt, excerpt.pointer)
        );
    } else {
        throw JsonLoadException(tfm::format("Encountered an error at %s:\n    %s", _file.fileName(), description), "");
    }
}

}
