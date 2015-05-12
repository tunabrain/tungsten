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
    std::unique_ptr<PathVertex[]> _vertices;
    std::unique_ptr<PathEdge[]> _edges;

public:
    LightPath(int maxLength)
    : _maxLength(maxLength),
      _length(0),
      _vertices(new PathVertex[maxLength + 4]),
      _edges(new PathEdge[maxLength + 4])
    {
    }

    void startCameraPath(const Camera *camera, Vec2u pixel)
    {
        _vertices[0] = PathVertex(camera, pixel);
    }

    void startEmitterPath(const Primitive *emitter, float emitterPdf)
    {
        _vertices[0] = PathVertex(emitter, emitterPdf);
    }

    void tracePath(const TraceableScene &scene, TraceBase &tracer, SampleGenerator &sampler,
            UniformSampler &supplementalSampler);

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

    Vec3f weightedPathEmission(int minLength, int maxLength) const;

    static Vec3f connect(const TraceableScene &scene, const PathVertex &a, const PathVertex &b);
    static bool connect(const TraceableScene &scene, const PathVertex &a, const PathVertex &b,
            SampleGenerator &sampler, Vec3f &weight, Vec2u &pixel);
    static float misWeight(const LightPath &camera, const LightPath &light, int s, int t);
};

}

#endif /* LIGHTPATH_HPP_ */
