#ifndef OUTPUTBUFFER_HPP_
#define OUTPUTBUFFER_HPP_

#include "OutputBufferSettings.hpp"

#include "math/Vec.hpp"
#include "math/Ray.hpp"

#include "io/JsonSerializable.hpp"
#include "io/FileUtils.hpp"
#include "io/ImageIO.hpp"

#include "Memory.hpp"

#include <memory>

namespace Tungsten {

template<typename T>
class OutputBuffer
{
    Vec2u _res;

    std::unique_ptr<T[]> _bufferA, _bufferB;
    std::unique_ptr<T[]> _variance;
    std::unique_ptr<uint32[]> _sampleCount;

    const OutputBufferSettings &_settings;

    inline float average(float x) const
    {
        return x;
    }
    inline float average(const Vec3f &x) const
    {
        return x.avg();
    }

    const float *elementPointer(const float *p) const
    {
        return p;
    }
    const float *elementPointer(const Vec3f *p) const
    {
        return p->data();
    }

    int elementCount(float /*x*/) const
    {
        return 1;
    }
    int elementCount(Vec3f /*x*/) const
    {
        return 3;
    }

    template<typename Texel>
    void saveLdr(const Texel *hdr, const Path &path, bool rescale) const
    {
        uint32 pixelCount = _res.product();
        std::unique_ptr<Vec3c[]> ldr(new Vec3c[pixelCount]);

        Texel minimum, maximum;
        if (_settings.type() == OutputDepth) {
            minimum = maximum = Texel(0.0f);
            for (uint32 i = 0; i < pixelCount; ++i)
                if (average(hdr[i]) != Ray::infinity())
                    maximum = max(maximum, hdr[i]);
        } else if (_settings.type() == OutputNormal) {
            minimum = Texel(-1.0f);
            maximum = Texel(1.0f);
        } else {
            rescale = false;
        }

        for (uint32 i = 0; i < pixelCount; ++i) {
            Texel f = hdr[i];
            if (rescale)
                f = (f - minimum)/(maximum - minimum);
            if (std::isnan(average(f)) || std::isinf(average(f)))
                ldr[i] = Vec3c(255);
            else
                ldr[i] = Vec3c(clamp(Vec3i(Vec3f(f*255.0f)), Vec3i(0), Vec3i(255)));
        }

        ImageIO::saveLdr(path, &ldr[0].x(), _res.x(), _res.y(), 3);
    }

public:
    OutputBuffer(Vec2u res, const OutputBufferSettings &settings)
    : _res(res),
      _settings(settings)
    {
        size_t numPixels = res.product();

        _bufferA = zeroAlloc<T>(numPixels);
        if (settings.twoBufferVariance())
            _bufferB = zeroAlloc<T>(numPixels);
        if (settings.sampleVariance())
            _variance = zeroAlloc<T>(numPixels);
        _sampleCount = zeroAlloc<uint32>(numPixels);
    }

    void addSample(Vec2u pixel, T c)
    {
        if (std::isnan(c) || std::isinf(c))
            return;

        int idx = pixel.x() + pixel.y()*_res.x();
        uint32 sampleIdx = _sampleCount[idx]++;
        if (_variance) {
            T curr;
            if (_bufferB && sampleIdx > 0) {
                uint32 sampleCountA = (sampleIdx + 1)/2;
                uint32 sampleCountB = sampleIdx/2;
                curr = (_bufferA[idx]*sampleCountA + _bufferB[idx]*sampleCountB)/sampleIdx;
            } else {
                curr = _bufferA[idx];
            }
            T delta = c - curr;
            curr += delta/(sampleIdx + 1);
            _variance[idx] += delta*(c - curr);
        }

        if (_bufferB) {
            T *feature = (sampleIdx & 1) ? _bufferB.get() : _bufferA.get();
            uint32 perBufferSampleCount = sampleIdx/2 + 1;
            feature[idx] += (c - feature[idx])/perBufferSampleCount;
        } else {
            _bufferA[idx] += (c - _bufferA[idx])/(sampleIdx + 1);
        }
    }

    inline T operator[](uint32 idx) const
    {
        if (_bufferB) {
            uint32 sampleIdx = _sampleCount[idx];
            uint32 sampleCountA = (sampleIdx + 1)/2;
            uint32 sampleCountB = sampleIdx/2;
            return (_bufferA[idx]*sampleCountA + _bufferB[idx]*sampleCountB)/float(max(sampleIdx, uint32(1)));
        } else {
            return _bufferA[idx];
        }
    }

    void save() const
    {
        Path ldrFile = _settings.ldrOutputFile();
        Path hdrFile = _settings.hdrOutputFile();
        Path ldrVariance = ldrFile.stripExtension() + "Variance" + ldrFile.extension();
        Path hdrVariance = hdrFile.stripExtension() + "Variance" + hdrFile.extension();
        Path ldrFileA = ldrFile.stripExtension() + "A" + ldrFile.extension();
        Path hdrFileA = hdrFile.stripExtension() + "A" + hdrFile.extension();
        Path ldrFileB = ldrFile.stripExtension() + "B" + ldrFile.extension();
        Path hdrFileB = hdrFile.stripExtension() + "B" + hdrFile.extension();

        uint32 numPixels = _res.product();
        if (_bufferB) {
            std::unique_ptr<T[]> hdr(new T[numPixels]);
            for (uint32 i = 0; i < numPixels; ++i)
                hdr[i] = (*this)[i];

            if (!hdrFile.empty()) {
                ImageIO::saveHdr(hdrFile,  elementPointer(hdr.get()),      _res.x(), _res.y(), elementCount(hdr[0]));
                ImageIO::saveHdr(hdrFileA, elementPointer(_bufferA.get()), _res.x(), _res.y(), elementCount(_bufferA[0]));
                ImageIO::saveHdr(hdrFileB, elementPointer(_bufferB.get()), _res.x(), _res.y(), elementCount(_bufferB[0]));
            }
            if (!ldrFile.empty()) {
                saveLdr(hdr.get(), ldrFile, true);
                saveLdr(_bufferA.get(), ldrFileA, true);
                saveLdr(_bufferB.get(), ldrFileB, true);
            }
        } else {
            if (!hdrFile.empty())
                ImageIO::saveHdr(hdrFile, elementPointer(_bufferA.get()), _res.x(), _res.y(), elementCount(_bufferA[0]));
            if (!ldrFile.empty())
                saveLdr(_bufferA.get(), ldrFile, true);
        }
        if (_variance) {
            std::unique_ptr<T[]> scaled(new T[numPixels]);
            for (uint32 i = 0; i < numPixels; ++i)
                scaled[i] = _variance[i]/T(_sampleCount[i]*max(uint32(1), _sampleCount[i] - 1));

            if (!hdrFile.empty())
                ImageIO::saveHdr(hdrVariance, elementPointer(scaled.get()), _res.x(), _res.y(), elementCount(T(0.0f)));
            if (!ldrFile.empty())
                saveLdr(scaled.get(), ldrVariance, false);
        }
    }

    void deserialize(InputStreamHandle &in) const
    {
        size_t numPixels = _res.product();
        FileUtils::streamRead(in, _bufferA.get(), numPixels);
        if (_bufferB)
            FileUtils::streamRead(in, _bufferB.get(), numPixels);
        if (_variance)
            FileUtils::streamRead(in, _variance.get(), numPixels);
        FileUtils::streamRead(in, _sampleCount.get(), numPixels);
    }

    void serialize(OutputStreamHandle &out) const
    {
        size_t numPixels = _res.product();
        FileUtils::streamWrite(out, _bufferA.get(), numPixels);
        if (_bufferB)
            FileUtils::streamWrite(out, _bufferB.get(), numPixels);
        if (_variance)
            FileUtils::streamWrite(out, _variance.get(), numPixels);
        FileUtils::streamWrite(out, _sampleCount.get(), numPixels);
    }

    inline T variance(int x, int y) const
    {
        return _variance[x + y*_res.x()]/max(uint32(1), _sampleCount[x + y*_res.x()] - 1);
    }
};

typedef OutputBuffer<float> OutputBufferF;
typedef OutputBuffer<Vec3f> OutputBufferVec3f;

}

#endif /* OUTPUTBUFFER_HPP_ */
