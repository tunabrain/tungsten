#include "Instance.hpp"
#include "TriangleMesh.hpp"

#include "sampling/PathSampleGenerator.hpp"
#include "sampling/SampleWarp.hpp"

#include "bsdfs/NullBsdf.hpp"

#include "io/JsonObject.hpp"
#include "io/Scene.hpp"

#include <iomanip>

namespace Tungsten {

void Instance::buildProxy()
{
    std::vector<Vec3f> masterSize;
    for (const auto &m : _master) {
        m->prepareForRender();
        masterSize.emplace_back(m->bounds().diagonal()/m->transform().extractScaleVec());
    }

    std::vector<Vertex> verts(_instanceCount*4);
    std::vector<TriangleI> tris(_instanceCount*2);
    for (uint32 i = 0; i < _instanceCount; ++i) {
        Vec3f diag = masterSize[_instanceId[i]];
        float size = diag.length();

        Vec3f v0 = _master[_instanceId[i]]->transform()*Vec3f( size, 0.0f,  size);
        Vec3f v1 = _master[_instanceId[i]]->transform()*Vec3f(-size, 0.0f,  size);
        Vec3f v2 = _master[_instanceId[i]]->transform()*Vec3f(-size, 0.0f, -size);
        Vec3f v3 = _master[_instanceId[i]]->transform()*Vec3f( size, 0.0f, -size);
        v0 = _instancePos[i] + _instanceRot[i]*v0;
        v1 = _instancePos[i] + _instanceRot[i]*v1;
        v2 = _instancePos[i] + _instanceRot[i]*v2;
        v3 = _instancePos[i] + _instanceRot[i]*v3;

        verts[i*4 + 0] = v0;
        verts[i*4 + 1] = v1;
        verts[i*4 + 2] = v2;
        verts[i*4 + 3] = v3;
        tris[i*2 + 0] = TriangleI(i*4 + 0, i*4 + 1, i*4 + 2, _instanceId[i]);
        tris[i*2 + 1] = TriangleI(i*4 + 0, i*4 + 2, i*4 + 3, _instanceId[i]);
    }

    _proxy = std::make_shared<TriangleMesh>(std::move(verts), std::move(tris), std::make_shared<NullBsdf>(), "Instances", false, false);
}

const Primitive &Instance::getMaster(const IntersectionTemporary &data) const
{
    return *_master[_instanceId[data.flags]];
}

float Instance::powerToRadianceFactor() const
{
    return 0.0f;
}

void Instance::fromJson(JsonPtr value, const Scene &scene)
{
    Primitive::fromJson(value, scene);

    _master.clear();
    if (auto master = value["masters"])
        for (unsigned i = 0; i < master.size(); ++i)
            _master.emplace_back(scene.fetchPrimitive(master[i]));

    if (auto instances = value["instances"]) {
        if (instances.isString()) {
            _instanceFileA = scene.fetchResource(instances);
        } else {
            _instanceFileA = nullptr;

            _instanceCount = instances.size();
            _instancePos = zeroAlloc<Vec3f>(_instanceCount);
            _instanceRot = zeroAlloc<QuaternionF>(_instanceCount);
            _instanceId = zeroAlloc<uint8>(_instanceCount);

            for (unsigned i = 0; i < _instanceCount; ++i) {
                instances[i].getField("id", _instanceId[i]);
                Mat4f transform;
                instances[i].getField("transform", transform);
                _instancePos[i] = transform.extractTranslationVec();
                _instanceRot[i] = QuaternionF::fromMatrix(transform.extractRotation());
            }
        }
    }
    if (auto instanceA = value["instancesA"]) _instanceFileA = scene.fetchResource(instanceA);
    if (auto instanceB = value["instancesB"]) _instanceFileB = scene.fetchResource(instanceB);

    value.getField("ratio", _ratio);
}

rapidjson::Value Instance::toJson(Allocator &allocator) const
{
    rapidjson::Value masters;
    masters.SetArray();
    for (const auto &m : _master)
        masters.PushBack(m->toJson(allocator), allocator);

    JsonObject result{Primitive::toJson(allocator), allocator,
        "type", "instances",
        "masters", std::move(masters),
        "ratio", _ratio
    };

    if (_instanceFileB) {
        result.add("instancesB", *_instanceFileB);
        if (_instanceFileA)
            result.add("instancesA", *_instanceFileA);
    } else if (_instanceFileA) {
        result.add("instances", *_instanceFileA);
    } else {
        rapidjson::Value instances;
        instances.SetArray();
        for (uint32 i = 0; i < _instanceCount; ++i) {
            JsonObject instance{allocator,
                "id", _instanceId[i],
                "transform", Mat4f::translate(_instancePos[i])*_instanceRot[i].toMatrix()
            };
            instances.PushBack(rapidjson::Value(instance), allocator);
        }
        result.add("instances", std::move(instances));
    }

    return result;
}

const uint32 CompressionLossy = 1;
const uint32 CompressionLZO = 2;

void loadLossyInstance(InputStreamHandle &in, const Box3f &bounds, Vec3f &pos, QuaternionF &f)
{
    CONSTEXPR uint32 PosW = 21;
    CONSTEXPR uint32 RotW = 8;
    CONSTEXPR uint32 AxisW = 12;

    uint32 a, b, c;
    FileUtils::streamRead(in, a);
    FileUtils::streamRead(in, b);
    FileUtils::streamRead(in, c);

    uint32 mask = ((1 << 21) - 1);
    uint32 x = (a >> 11);
    uint32 y = ((a << 10) | (b >> 22)) & mask;
    uint32 z = ((b >> 1) & mask);

    uint32 rot = c & ((1 << RotW) - 1);
    uint32 axisX = (c >> RotW) & ((1 << AxisW) - 1);
    uint32 axisY = (c >> (RotW + AxisW)) & ((1 << AxisW) - 1);

    float axisXf = (axisX/float(1 << AxisW))*2.0f - 1.0f;
    float axisYf = (axisY/float(1 << AxisW))*2.0f - 1.0f;
    float rotW = TWO_PI*rot/(1 << RotW);
    Vec3f w(axisXf, axisYf, std::sqrt(max(1 - sqr(axisXf) - sqr(axisYf), 0.0f)));

    pos = lerp(bounds.min(), bounds.max(), Vec3f(Vec3u(x, y, z))/float(1 << PosW));
    f = QuaternionF(rotW, w);
}
void loadLosslessInstance(InputStreamHandle &in, Vec3f &pos, QuaternionF &f)
{
    FileUtils::streamRead(in, pos);
    Vec3f w;
    FileUtils::streamRead(in, w);

    float angle = w.length();
    w = (angle > 0 ? w/angle : Vec3f(0.0f, 1.0f, 0.0f));
    f = QuaternionF(angle, w);
}

void saveLossyInstance(OutputStreamHandle &out, const Box3f &bounds, Vec3f pos, QuaternionF f)
{
    CONSTEXPR uint32 RotW = 8;
    CONSTEXPR uint32 AxisW = 12;

    Vec3u xyz(clamp(Vec3i(((pos - bounds.min())/(bounds.max() - bounds.min()))*(1 << 21)), Vec3i(0), Vec3i((1 << 21) - 1)));
    uint32 a, b;
    a = (xyz[0] << 11) | (xyz[1] >> 10);
    b = (xyz[1] << 22) | (xyz[2] << 1);

    float angle = std::acos(clamp(f.x(), -1.0f, 1.0f))*2.0f;
    Vec3f w = Vec3f(f[1], f[2], f[3]).normalized();
    if (f[3] < 0) {
        w = -w;
        angle = TWO_PI - angle;
    }

    uint32 rot = clamp(int(angle/TWO_PI*(1 << RotW)), 0, (1 << RotW) - 1);
    uint32 axisX = clamp(int((w[0]*0.5f + 0.5f)*(1 << AxisW)), 0, (1 << AxisW) - 1);
    uint32 axisY = clamp(int((w[1]*0.5f + 0.5f)*(1 << AxisW)), 0, (1 << AxisW) - 1);

    uint32 c = (axisY << (RotW + AxisW)) | (axisX << RotW) | rot;

    FileUtils::streamWrite(out, a);
    FileUtils::streamWrite(out, b);
    FileUtils::streamWrite(out, c);
}
void saveLosslessInstance(OutputStreamHandle &out, Vec3f pos, QuaternionF f)
{
    FileUtils::streamWrite(out, pos);
    float angle = std::acos(clamp(f.x(), -1.0f, 1.0f))*2.0f;
    Vec3f w = Vec3f(f[1], f[2], f[3]).normalized()*angle;
    FileUtils::streamWrite(out, w);
}

bool loadInstances(const Path &path, Box3f &bounds, uint32 &instanceCount, std::unique_ptr<Vec3f[]> &instancePos, std::unique_ptr<QuaternionF[]> &instanceRot, std::unique_ptr<uint8[]> &instanceId)
{
    InputStreamHandle in = FileUtils::openInputStream(path);
    if (!in)
        return false;

    FileUtils::streamRead(in, instanceCount);
    uint32 compressed;
    FileUtils::streamRead(in, compressed);

    FileUtils::streamRead(in, bounds);

    instancePos.reset(new Vec3f[instanceCount]);
    instanceRot.reset(new QuaternionF[instanceCount]);
    instanceId.reset(new uint8[instanceCount]);

    if (compressed & CompressionLossy)
        for (uint32 i = 0; i < instanceCount; ++i)
            loadLossyInstance(in, bounds, instancePos[i], instanceRot[i]);
    else
        for (uint32 i = 0; i < instanceCount; ++i)
            loadLosslessInstance(in, instancePos[i], instanceRot[i]);
    FileUtils::streamRead(in, instanceId.get(), instanceCount);

    return true;
}

bool saveInstances(const Path &path, uint32 instanceCount, const Vec3f *instancePos, const QuaternionF *instanceRot, const uint8 *instanceId, bool compress)
{
    OutputStreamHandle out = FileUtils::openOutputStream(path);
    if (!out)
        return false;

    FileUtils::streamWrite(out, instanceCount);
    FileUtils::streamWrite(out, uint32(compress ? CompressionLossy : 0));

    Box3f bounds;
    for (uint32 i = 0; i < instanceCount; ++i)
        bounds.grow(instancePos[i]);
    FileUtils::streamWrite(out, bounds);

    if (compress)
        for (uint32 i = 0; i < instanceCount; ++i)
            saveLossyInstance(out, bounds, instancePos[i], instanceRot[i]);
    else
        for (uint32 i = 0; i < instanceCount; ++i)
            saveLosslessInstance(out, instancePos[i], instanceRot[i]);
    FileUtils::streamWrite(out, instanceId, instanceCount);

    return true;
}

Instance::Instance()
: _ratio(0),
  _instanceCount(0)
{
}

void Instance::loadResources()
{
    Box3f bounds;
    if (_instanceFileA)
        loadInstances(*_instanceFileA, bounds, _instanceCount, _instancePos, _instanceRot, _instanceId);
    if (_instanceFileB) {
        uint32 instanceCountB;
        std::unique_ptr<Vec3f[]> instancePosB;
        std::unique_ptr<QuaternionF[]> instanceRotB;
        std::unique_ptr<uint8[]> instanceIdB;
        if (loadInstances(*_instanceFileB, bounds, instanceCountB, instancePosB, instanceRotB, instanceIdB) && _instanceCount == instanceCountB) {
            for (uint32 i = 0; i < _instanceCount; ++i) {
                _instancePos[i] = lerp(_instancePos[i], instancePosB[i], _ratio);
                _instanceRot[i] = _instanceRot[i].slerp(instanceRotB[i], _ratio);
            }
        }
    }
}

void Instance::saveResources()
{
    if (_instanceFileA && !_instanceFileB)
        saveInstances(*_instanceFileA, _instanceCount, _instancePos.get(), _instanceRot.get(), _instanceId.get(), false);
}

bool Instance::intersect(Ray &ray, IntersectionTemporary &data) const
{
    bool hit = false;
    uint32 prim;
    _bvh->trace(ray, [&](Ray &ray, uint32 id, float tMin, const Vec3pf &/*bounds*/) {
        QuaternionF invRot = _instanceRot[id].conjugate();
        Ray localRay = ray.scatter(invRot*(ray.pos() - _instancePos[id]), invRot*ray.dir(), tMin);
        bool hitI = _master[_instanceId[id]]->intersect(localRay, data);
        if (hitI) {
            hit = true;
            prim = id;
            ray.setFarT(localRay.farT());
        }
    });

    if (hit) {
        data.primitive = this;
        data.flags = prim;
    }

    return hit;
}

bool Instance::occluded(const Ray &ray) const
{
    Ray tmpRay = ray;
    bool occluded = false;
    _bvh->trace(tmpRay, [&](Ray &ray, uint32 id, float tMin, const Vec3pf &/*bounds*/) {
        QuaternionF invRot = _instanceRot[id].conjugate();
        Ray localRay = ray.scatter(invRot*(ray.pos() - _instancePos[id]), invRot*ray.dir(), tMin);
        bool occludedI = _master[_instanceId[id]]->occluded(localRay);
        if (occludedI) {
            occluded = true;
            ray.setFarT(ray.farT() - 1);
        }
    });

    return occluded;
}

bool Instance::hitBackside(const IntersectionTemporary &data) const
{
    return getMaster(data).hitBackside(data);
}

void Instance::intersectionInfo(const IntersectionTemporary &data, IntersectionInfo &info) const
{
    getMaster(data).intersectionInfo(data, info);

    QuaternionF rot = _instanceRot[data.flags];
    info.Ng = rot*info.Ng;
    info.Ns = rot*info.Ns;
    info.p = _instancePos[data.flags] + rot*info.p;
    info.primitive = this;
}

bool Instance::tangentSpace(const IntersectionTemporary &/*data*/, const IntersectionInfo &/*info*/, Vec3f &/*T*/, Vec3f &/*B*/) const
{
    return false;
}

bool Instance::isSamplable() const
{
    return false;
}

void Instance::makeSamplable(const TraceableScene &/*scene*/, uint32 /*threadIndex*/)
{
}

bool Instance::invertParametrization(Vec2f /*uv*/, Vec3f &/*pos*/) const
{
    return false;
}

bool Instance::isDirac() const
{
    return false;
}

bool Instance::isInfinite() const
{
    return false;
}

float Instance::approximateRadiance(uint32 /*threadIndex*/, const Vec3f &/*p*/) const
{
    return -1.0f;
}

Box3f Instance::bounds() const
{
    return _bounds;
}

const TriangleMesh &Instance::asTriangleMesh()
{
    if (!_proxy)
        buildProxy();
    return *_proxy;
}

void Instance::prepareForRender()
{
    for (auto &m : _master)
        m->prepareForRender();

    Bvh::PrimVector prims;
    prims.reserve(_instanceCount);

    auto rot = QuaternionF::fromMatrix(_transform.extractRotation());
    for (uint32 i = 0; i < _instanceCount; ++i) {
        _instancePos[i] = _transform*_instancePos[i];
        _instanceRot[i] = rot*_instanceRot[i];
    }

    std::vector<Box3f> masterBounds;
    for (const auto &m : _master)
        masterBounds.emplace_back(m->bounds());

    _bounds = Box3f();
    for (uint32 i = 0; i < _instanceCount; ++i) {
        Box3f bLocal = masterBounds[_instanceId[i]];

        Box3f bGlobal;
        for (float x : {0, 1})
            for (float y : {0, 1})
                for (float z : {0, 1})
                    bGlobal.grow(_instancePos[i] + _instanceRot[i]*lerp(bLocal.min(), bLocal.max(), Vec3f(x, y, z)));

        _bounds.grow(bGlobal);

        prims.emplace_back(Bvh::Primitive(bGlobal, bGlobal.center(), i));
    }

    _bvh.reset(new Bvh::BinaryBvh(std::move(prims), 2));

    Primitive::prepareForRender();
}

void Instance::teardownAfterRender()
{
    _bvh.reset();
    loadResources();

    Primitive::teardownAfterRender();
}

int Instance::numBsdfs() const
{
    return _master.size();
}

std::shared_ptr<Bsdf> &Instance::bsdf(int index)
{
    return _master[index]->bsdf(0);
}

void Instance::setBsdf(int index, std::shared_ptr<Bsdf> &bsdf)
{
    _master[index]->setBsdf(0, bsdf);
}

Primitive *Instance::clone()
{
    return nullptr;
}

}
