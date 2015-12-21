#ifndef CUBICELEMENT_HPP_
#define CUBICELEMENT_HPP_

#include "TexturedQuad.hpp"
#include "NamedFace.hpp"
#include "CubeFace.hpp"

#include "math/Mat4f.hpp"
#include "math/Vec.hpp"

#include "io/JsonUtils.hpp"

#include <string>
#include <vector>

namespace Tungsten {
namespace MinecraftLoader {

class CubicElement
{
    struct OptionalFace
    {
        CubeFace face;
        bool filled = false;
    };

    Vec3f _from;
    Vec3f _to;

    int _rotAxis;
    Vec3f _rotOrigin;
    float _rotAngle;
    bool _rotRescale;

    bool _shade;

    OptionalFace _faces[6];

    void loadFaces(const rapidjson::Value &faces)
    {
        for (int i = 0; i < 6; ++i) {
            auto face = faces.FindMember(cubeFaceToString(NamedFace(i)));

            if (face != faces.MemberEnd() && face->value.IsObject()) {
                _faces[i].filled = true;
                _faces[i].face = CubeFace(face->value);
            }
        }
    }

public:
    CubicElement(const rapidjson::Value &v)
    : _from(0.0f),
      _to(0.0f),
      _rotAxis(-1),
      _rotOrigin(8.0f),
      _rotAngle(0.0f),
      _rotRescale(false),
      _shade(true)
    {
        JsonUtils::fromJson(v, "from", _from);
        JsonUtils::fromJson(v, "to", _to);
        JsonUtils::fromJson(v, "shade", _shade);

        auto rotation = v.FindMember("rotation");
        auto faces    = v.FindMember("faces");

        if (faces != v.MemberEnd() && faces->value.IsObject())
            loadFaces(faces->value);

        if (rotation != v.MemberEnd() && rotation->value.IsObject()) {
            JsonUtils::fromJson(rotation->value, "origin", _rotOrigin);
            JsonUtils::fromJson(rotation->value, "angle", _rotAngle);
            JsonUtils::fromJson(rotation->value, "rescale", _rotRescale);

            std::string axis;
            if (JsonUtils::fromJson(rotation->value, "axis", axis)) {
                if (axis == "x")
                    _rotAxis = 0;
                else if (axis == "y")
                    _rotAxis = 1;
                else if (axis == "z")
                    _rotAxis = 2;
            }
        }
    }

    void instantiateQuads(std::vector<TexturedQuad> &quads) const
    {
        Mat4f tform;
        if (_rotAxis != -1) {
            Vec3f rot(0.0f);
            rot[_rotAxis] = _rotAngle;
            tform = Mat4f::rotXYZ(rot);
            if (_rotRescale) {
                Vec3f scale(1.0f/std::sqrt(std::abs(std::cos(_rotAngle))));
                scale[_rotAxis] = 1.0f;
                tform = Mat4f::scale(scale)*tform;
            }
            tform = Mat4f::translate(Vec3f(_rotOrigin))*tform*Mat4f::translate(Vec3f(-_rotOrigin));
        }

        const Vec3f faceVerts[6][4] = {
            {{0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}},
            {{0.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
            {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
            {{1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}},
        };

        Vec3f scale(_to - _from);
        for (int i = 0; i < 6; ++i) {
            Vec3f base(_from);
            if (i % 2)
                base[i/2] = _to[i/2];
            if (_faces[i].filled) {
                auto uvs = _faces[i].face.generateUVs();

                quads.push_back(TexturedQuad{
                    _faces[i].face.texture(),
                    "",
                    _faces[i].face.tint(),
                    tform*(faceVerts[i][0]*scale + base), uvs[0],
                    tform*(faceVerts[i][1]*scale + base), uvs[1],
                    tform*(faceVerts[i][2]*scale + base), uvs[2],
                    tform*(faceVerts[i][3]*scale + base), uvs[3],
                });
            }
        }
    }
};

}
}

#endif /* CUBICELEMENT_HPP_ */
