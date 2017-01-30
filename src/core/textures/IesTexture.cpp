#include "IesTexture.hpp"

#include "math/MathUtil.hpp"
#include "math/Angle.hpp"

#include "io/JsonObject.hpp"
#include "io/Scene.hpp"

#include "Debug.hpp"

#include <iostream>

namespace Tungsten {

IesTexture::IesTexture()
: _path(std::make_shared<Path>("")),
  _resolution(256)
{
}

IesTexture::IesTexture(PathPtr path, int resolution)
: _path(std::move(path)),
  _resolution(resolution)
{
}

void IesTexture::fromJson(JsonPtr value, const Scene &scene)
{
    if (auto path = value["file"])
        _path = scene.fetchResource(path);
    value.getField("resolution", _resolution);
}

rapidjson::Value IesTexture::toJson(Allocator &allocator) const
{
    JsonObject result{Texture::toJson(allocator), allocator,
        "type", "ies",
        "resolution", _resolution
    };
    if (_path)
        result.add("file", *_path);
    return result;
}

void wrapHorzAngles(int type, std::vector<float> &angles, std::vector<int> &indices)
{
    if (type == 1) {
        if (angles.back() == 0.0f) {
            angles.clear();
            angles.push_back(0.0f);
            angles.push_back(360.0f);
            indices.clear();
            indices.push_back(0);
            indices.push_back(0);
        }
        if (angles.back() == 90.0f) {
            int numEntries = angles.size();
            for (int i = numEntries - 2; i >= 0; --i) {
                angles.push_back(180.0f - angles[i]);
                indices.push_back(indices[i]);
            }
            angles.back() = 180.0f; // Make sure it's exactly 180
        }
        if (angles.back() == 180.0f) {
            int numEntries = angles.size();
            for (int i = numEntries - 2; i >= 0; --i) {
                angles.push_back(360.0f - angles[i]);
                indices.push_back(indices[i]);
            }
            angles.back() = 360.0f; // Make sure it's exactly 360
        }
    }
}

// The IES format does not allow for comma separated values,
// but sadly this doesn't stop certain manufacturers from adding
// commas anyway. We ignore them here.
static float readFloat(std::istream &i)
{
    i >> std::skipws;
    if (i.peek() == ',')
        i.get();

    float result;
    i >> result;
    return result;
}

void IesTexture::loadResources()
{
    std::string data;
    if (_path)
        data = FileUtils::loadText(*_path);
    std::vector<float> vertAngles, horzAngles, candelas;
    int photometricType, verticalCount;

    std::istringstream in(data);
    std::string line;
    while (std::getline(in, line)) {
        if (line.find("TILT=") != std::string::npos) {
            if (line.find("TILT=INCLUDE") != std::string::npos) {
                // Skip TILT data
                std::getline(in, line);
                int numAngles;
                in >> numAngles;
                float dummy;
                for (int i = 0; i < numAngles*2; ++i)
                    in >> dummy;
            }

            int numLamps, horizontalCount, unitType;
            float lumensPerLamp, candelaMultiplier, width, height, length, ballast, future, watts;
            in >> numLamps >> lumensPerLamp >> candelaMultiplier >> verticalCount >> horizontalCount
               >> photometricType >> unitType >> width >> height >> length
               >> ballast >> future >> watts;

            vertAngles.reserve(verticalCount);
            for (int i = 0; i < verticalCount; ++i)
                vertAngles.push_back(readFloat(in));
            horzAngles.reserve(horizontalCount);
            for (int i = 0; i < horizontalCount; ++i)
                horzAngles.push_back(readFloat(in));
            candelas.reserve(verticalCount*horizontalCount);
            for (int i = 0; i < verticalCount*horizontalCount; ++i)
                candelas.push_back(readFloat(in));

            break;
        }
    }

    std::vector<int> horzIndex, vertIndex;
    for (size_t i = 0; i < horzAngles.size(); ++i)
        horzIndex.push_back(i);
    for (size_t i = 0; i < vertAngles.size(); ++i)
        vertIndex.push_back(i);

    wrapHorzAngles(photometricType, horzAngles, horzIndex);

    std::unique_ptr<float[]> texels(new float[_resolution*_resolution*2]);

    if (horzAngles.empty() || vertAngles.empty()) {
        if (_path && !_path->empty())
            DBG("Unable to load IES profile at '%s'", *_path);

        for (int i = 0; i < _resolution*_resolution*2; ++i)
            texels[i] = INV_TWO_PI;

        init(texels.release(), _resolution*2, _resolution, getTexelType(false, true));
        return;
    }

    float maxValue = 0.0f;
    for (int y = 0; y < _resolution; ++y) {
        for (int x = 0; x < _resolution*2; ++x) {
            float u = (x + 0.5f)/(_resolution*2);
            float v = (y + 0.5f)/_resolution;
            float horz = u*360.0f;
            float vert = (1.0f - v)*180.0f;

            if (photometricType != 1) {
                if (horz > 180.0f)
                    horz -= 360.0f;
                if (vert > 90.0f)
                    vert -= 180.0f;
            }

            int row0 = -1, row1 = -1, col0 = -1, col1 = -1;
            if (photometricType == 1 || (horz >= horzAngles.front() && horz <= horzAngles.back())) {
                auto top = std::lower_bound(horzAngles.begin(), horzAngles.end(), horz);
                if (top == horzAngles.end())
                    top = std::lower_bound(horzAngles.begin(), horzAngles.end(), horz - 360.0f);
                if (top != horzAngles.end()) {
                    row1 = top - horzAngles.begin();
                    row0 = top == horzAngles.begin() ? (horzIndex.size() - 1) : (top - 1 - horzAngles.begin());
                }
            }
            if (vert >= vertAngles.front() && vert <= vertAngles.back()) {
                auto top = std::lower_bound(vertAngles.begin(), vertAngles.end(), vert);
                if (top != vertAngles.end()) {
                    col1 = top - vertAngles.begin();
                    col0 = top == vertAngles.begin() ? col1 : (top - 1 - vertAngles.begin());
                }
            }

            float value = 0.0f;
            if (row0 != -1 && row1 != -1 && col0 != -1 && col1 != -1) {
                float horz0 = horzAngles[row0];
                float horz1 = horzAngles[row1];
                float vert0 = vertAngles[col0];
                float vert1 = vertAngles[col1];
                if (horz0 > horz1)
                    horz0 -= 360.0f;

                float c00 = candelas[horzIndex[row0]*verticalCount + vertIndex[col0]];
                float c01 = candelas[horzIndex[row0]*verticalCount + vertIndex[col1]];
                float c10 = candelas[horzIndex[row1]*verticalCount + vertIndex[col0]];
                float c11 = candelas[horzIndex[row1]*verticalCount + vertIndex[col1]];

                float u = horz0 == horz1 ? 0.0f : (horz - horz0)/(horz1 - horz0);
                float v = vert0 == vert1 ? 0.0f : (vert - vert0)/(vert1 - vert0);
                value = (c00*(1.0f - u) + c10*u)*(1.0f - v)
                      + (c01*(1.0f - u) + c11*u)*v;
            }

            texels[x + y*_resolution*2] = value;
            maxValue = std::max(maxValue, value);
        }
    }
    if (maxValue != 0.0f)
        for (int i = 0; i < _resolution*_resolution*2; ++i)
            texels[i] /= maxValue;

    init(texels.release(), _resolution*2, _resolution, getTexelType(false, true));
}

Texture *IesTexture::clone() const
{
    return new IesTexture(*this);
}

}
