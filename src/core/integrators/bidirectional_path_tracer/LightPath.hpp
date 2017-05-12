#ifndef LIGHTPATH_HPP_
#define LIGHTPATH_HPP_

#include "PathVertex.hpp"
#include "PathEdge.hpp"

#include <memory>

namespace Tungsten {

class LightPath
{
    int _maxLength;
    int _maxVertices;
    int _length;
    bool _adjoint;
    std::unique_ptr<int[]> _vertexIndex;
    std::unique_ptr<PathVertex[]> _vertices;
    std::unique_ptr<PathEdge[]> _edges;

    float geometryFactor(int startVertex) const;
    float invGeometryFactor(int startVertex) const;

    void toAreaMeasure();

    static float misWeight(const LightPath &camera, const LightPath &emitter,
            const PathEdge &edge, int s, int t, float *ratios);

public:
    LightPath(int maxLength)
    : _maxLength(maxLength),
      _maxVertices(maxLength + 4),
      _length(0),
      _adjoint(false),
      _vertexIndex(new int[_maxVertices]),
      _vertices(new PathVertex[_maxVertices]),
      _edges(new PathEdge[_maxVertices])
    {
    }

    void clear()
    {
        _length = 0;
    }

    void resize(int length)
    {
        _length = length;
    }

    void startCameraPath(const Camera *camera)
    {
        _vertices[0] = PathVertex(camera);
        _length = 0;
        _adjoint = false;
    }

    void startCameraPath(const Camera *camera, Vec2u pixel)
    {
        _vertices[0] = PathVertex(camera, pixel);
        _length = 0;
        _adjoint = false;
    }

    void startEmitterPath(const Primitive *emitter, float emitterPdf)
    {
        _vertices[0] = PathVertex(emitter, emitterPdf);
        _length = 0;
        _adjoint = true;
    }

    void tracePath(const TraceableScene &scene, TraceBase &tracer, PathSampleGenerator &sampler, int length = -1, bool prunePath = true);

    int maxLength() const
    {
        return _maxLength;
    }

    int length() const
    {
        return _length;
    }

    PathVertex &operator[](int i)
    {
        return _vertices[i];
    }

    const PathVertex &operator[](int i) const
    {
        return _vertices[i];
    }

    PathEdge &edge(int i)
    {
        return _edges[i];
    }

    const PathEdge &edge(int i) const
    {
        return _edges[i];
    }

    int vertexIndex(int i) const
    {
        return _vertexIndex[i];
    }

    void prune();

    void copy(const LightPath &o);

    Vec3f bdptWeightedPathEmission(int minLength, int maxLength, float *ratios = nullptr, Vec3f *directEmissionByBounce = nullptr) const;

    static Vec3f bdptConnect(const TraceBase &tracer, const LightPath &camera, const LightPath &emitter,
            int s, int t, int maxBounce, PathSampleGenerator &sampler, float *ratios = nullptr);
    static bool bdptCameraConnect(const TraceBase &tracer, const LightPath &camera, const LightPath &emitter,
            int s, int maxBounce, PathSampleGenerator &sampler, Vec3f &weight, Vec2f &pixel, float *ratios = nullptr);

    bool extendSampleSpace(WritablePathSampleGenerator &sampler, const LightPath &source, int numVerts) const;

    static bool invert(WritablePathSampleGenerator &cameraSampler, WritablePathSampleGenerator &emitterSampler,
            const LightPath &camera, const LightPath &emitter, int newS);
};

}

#endif /* LIGHTPATH_HPP_ */
