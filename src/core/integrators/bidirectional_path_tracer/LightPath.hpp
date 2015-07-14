#ifndef LIGHTPATH_HPP_
#define LIGHTPATH_HPP_

#include "PathVertex.hpp"
#include "PathEdge.hpp"

#include <memory>

namespace Tungsten {

class LightPath
{
    int _maxLength;
    int _length;
    bool _adjoint;
    std::unique_ptr<int[]> _vertexIndex;
    std::unique_ptr<PathVertex[]> _vertices;
    std::unique_ptr<PathEdge[]> _edges;

    float geometryFactor(int startVertex) const;
    float invGeometryFactor(int startVertex) const;

    void toAreaMeasure();

    static float misWeight(const LightPath &camera, const LightPath &emitter,
            const PathEdge &edge, int s, int t);

public:
    LightPath(int maxLength)
    : _maxLength(maxLength),
      _length(0),
      _adjoint(false),
      _vertexIndex(new int[maxLength + 4]),
      _vertices(new PathVertex[maxLength + 4]),
      _edges(new PathEdge[maxLength + 4])
    {
    }

    void clear()
    {
        _length = 0;
    }

    void startCameraPath(const Camera *camera, Vec2u pixel)
    {
        _vertices[0] = PathVertex(camera, pixel);
        _adjoint = false;
    }

    void startEmitterPath(const Primitive *emitter, float emitterPdf)
    {
        _vertices[0] = PathVertex(emitter, emitterPdf);
        _adjoint = true;
    }

    void tracePath(const TraceableScene &scene, TraceBase &tracer, PathSampleGenerator &sampler, int length = -1);

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

    Vec3f bdptWeightedPathEmission(int minLength, int maxLength) const;

    static Vec3f bdptConnect(const TraceBase &tracer, const LightPath &camera, const LightPath &emitter,
            int s, int t, int maxBounce);
    static bool bdptCameraConnect(const TraceBase &tracer, const LightPath &camera, const LightPath &emitter,
            int s, int maxBounce, PathSampleGenerator &sampler, Vec3f &weight, Vec2f &pixel);
};

}

#endif /* LIGHTPATH_HPP_ */
