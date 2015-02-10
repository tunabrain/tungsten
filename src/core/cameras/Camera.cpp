#include "Camera.hpp"

#include "renderer/Renderer.hpp"

#include "math/Angle.hpp"
#include "math/Ray.hpp"

#include "io/FileUtils.hpp"
#include "io/JsonUtils.hpp"
#include "io/ImageIO.hpp"
#include "io/Scene.hpp"

#include <cmath>

namespace Tungsten {

// Default to low-res 16:9
Camera::Camera()
: Camera(Mat4f(), Vec2u(1000u, 563u), 32)
{
}

Camera::Camera(const Mat4f &transform, const Vec2u &res, uint32 spp)
: _outputFile("TungstenRender.png"),
  _tonemapString("gamma"),
  _overwriteOutputFiles(false),
  _transform(transform),
  _res(res),
  _spp(spp)
{
    _pos    = _transform*Vec3f(0.0f, 0.0f, 2.0f);
    _lookAt = _transform*Vec3f(0.0f, 0.0f, -1.0f);
    _up     = _transform*Vec3f(0.0f, 1.0f, 0.0f);

    precompute();
}

void Camera::precompute()
{
    _tonemapOp = Tonemap::stringToType(_tonemapString);
    _ratio = _res.y()/float(_res.x());
    _pixelSize = Vec2f(2.0f/_res.x(), 2.0f/_res.x());
    _transform = Mat4f::lookAt(_pos, _lookAt - _pos, _up);
    _invTransform = _transform.pseudoInvert();
}

Path Camera::incrementalFilename(const Path &dstFile, const std::string &suffix) const
{
    Path dstPath = (dstFile.stripExtension() + suffix) + dstFile.extension();
    if (_overwriteOutputFiles)
        return std::move(dstPath);

    Path barePath = dstPath.stripExtension();
    Path extension = dstPath.extension();

    int index = 0;
    while (dstPath.exists())
        dstPath = (barePath + tfm::format("%05d", ++index)) + extension;

    return std::move(dstPath);
}

void Camera::saveBuffers(Renderer &renderer, const std::string &suffix) const
{
    std::unique_ptr<Vec3f[]> hdr(new Vec3f[_res.x()*_res.y()]);
    std::unique_ptr<Vec3c[]> ldr(new Vec3c[_res.x()*_res.y()]);

    for (uint32 y = 0; y < _res.y(); ++y) {
        for (uint32 x = 0; x < _res.x(); ++x) {
            hdr[x + y*_res.x()] = getLinear(x, y);
            ldr[x + y*_res.x()] = Vec3c(clamp(Vec3i(get(x, y)*255.0f), Vec3i(0), Vec3i(255)));
        }
    }

    if (!_outputFile.empty())
        ImageIO::saveLdr(incrementalFilename(_outputFile, suffix), &ldr[0].x(), _res.x(), _res.y(), 3);
    if (!_hdrOutputFile.empty())
        ImageIO::saveHdr(incrementalFilename(_hdrOutputFile, suffix), &hdr[0].x(), _res.x(), _res.y(), 3);

    if (!_varianceOutputFile.empty()) {
        std::vector<float> variance;
        int varianceW, varianceH;
        renderer.getVarianceImage(variance, varianceW, varianceH);

        std::unique_ptr<uint8[]> ldrVariance(new uint8[_res.x()*_res.y()]);
        for (int y = 0; y < varianceH; ++y)
            for (int x = 0; x < varianceW; ++x)
                ldrVariance[x + y*varianceW] = clamp(int(variance[x + y*varianceW]*255.0f), 0, 255);

        ImageIO::saveLdr(incrementalFilename(_varianceOutputFile, suffix), ldrVariance.get(), varianceW, varianceH, 1);
    }
}

void Camera::fromJson(const rapidjson::Value &v, const Scene &scene)
{
    JsonUtils::fromJson(v, "output_file", _outputFile);
    JsonUtils::fromJson(v, "hdr_output_file", _hdrOutputFile);
    JsonUtils::fromJson(v, "variance_output_file", _varianceOutputFile);
    JsonUtils::fromJson(v, "overwrite_output_files", _overwriteOutputFiles);
    JsonUtils::fromJson(v, "tonemap", _tonemapString);
    JsonUtils::fromJson(v, "position", _pos);
    JsonUtils::fromJson(v, "lookAt", _lookAt);
    JsonUtils::fromJson(v, "up", _up);
    JsonUtils::fromJson(v, "resolution", _res);
    JsonUtils::fromJson(v, "spp", _spp);
    if (const rapidjson::Value::Member *medium = v.FindMember("medium"))
        _medium = scene.fetchMedium(medium->value);

    precompute();
}

rapidjson::Value Camera::toJson(Allocator &allocator) const
{
    rapidjson::Value v = JsonSerializable::toJson(allocator);
    if (!_outputFile.empty())
        v.AddMember("output_file", _outputFile.asString().c_str(), allocator);
    if (!_hdrOutputFile.empty())
        v.AddMember("hdr_output_file", _hdrOutputFile.asString().c_str(), allocator);
    if (!_varianceOutputFile.empty())
        v.AddMember("variance_output_file", _varianceOutputFile.asString().c_str(), allocator);
    v.AddMember("overwrite_output_files", _overwriteOutputFiles, allocator);
    v.AddMember("tonemap", _tonemapString.c_str(), allocator);
    v.AddMember("position", JsonUtils::toJsonValue<float, 3>(_pos,    allocator), allocator);
    v.AddMember("lookAt",   JsonUtils::toJsonValue<float, 3>(_lookAt, allocator), allocator);
    v.AddMember("up",       JsonUtils::toJsonValue<float, 3>(_up,     allocator), allocator);
    v.AddMember("resolution", JsonUtils::toJsonValue<uint32, 2>(_res, allocator), allocator);
    v.AddMember("spp", _spp, allocator);
    if (_medium)
        JsonUtils::addObjectMember(v, "medium", *_medium,  allocator);
    return std::move(v);
}

void Camera::prepareForRender()
{
    _pixels.resize(_res.x()*_res.y(), Vec3d(0.0));
    _weights.resize(_res.x()*_res.y(), 0.0);
}

void Camera::teardownAfterRender()
{
    _pixels.clear();
    _weights.clear();
    _pixels.shrink_to_fit();
    _weights.shrink_to_fit();
}

void Camera::setTransform(const Vec3f &pos, const Vec3f &lookAt, const Vec3f &up)
{
    _pos = pos;
    _lookAt = lookAt;
    _up = up;
    precompute();
}

void Camera::setPos(const Vec3f &pos)
{
    _pos = pos;
    precompute();
}

void Camera::setLookAt(const Vec3f &lookAt)
{
    _lookAt = lookAt;
    precompute();
}

void Camera::setUp(const Vec3f &up)
{
    _up = up;
    precompute();
}

void Camera::saveOutputs(Renderer &renderer) const
{
    saveBuffers(renderer, "");
}

void Camera::saveCheckpoint(Renderer &renderer) const
{
    saveBuffers(renderer, "_checkpoint");
}

}
