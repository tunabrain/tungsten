#ifndef ATMOSPHERICSCATTERING_HPP_
#define ATMOSPHERICSCATTERING_HPP_

#include "math/Vec.hpp"

namespace Tungsten
{

struct AtmosphereParameters {
    float albedo;

    float rayleighH;
    Vec3f rayleighSigmaS;

    float mieH;
    Vec3f mieSigmaS;
    Vec3f mieSigmaT;
    float mieG, mieGSq;

    AtmosphereParameters(float alpha, float rH, const Vec3f &rSS, float mH, const Vec3f &mieSS, const Vec3f &mieST, float mG) :
        albedo(alpha), rayleighH(rH), rayleighSigmaS(rSS), mieH(mH), mieSigmaS(mieSS), mieSigmaT(mieST), mieG(mG), mieGSq(mG*mG) {}

    static const AtmosphereParameters generic() {
        return AtmosphereParameters(
            0.1f,
            8.0f, Vec3f(5.8e-3f, 1.35e-2f, 3.31e-2f),
            1.2f, Vec3f(4e-3f), Vec3f(4e-3f)/0.9f, 0.8f
        );
    }

    static const AtmosphereParameters clearSky() {
        return AtmosphereParameters(
            0.1f,
            8.0f, Vec3f(5.8e-3f, 1.35e-2f, 3.31e-2f),
            1.2f, Vec3f(20e-3f), Vec3f(20e-3f)/0.9f, 0.76f
        );
    }

    static const AtmosphereParameters partlyCloudy() {
        return AtmosphereParameters(
            0.1f,
            8.0f, Vec3f(5.8e-3f, 1.35e-2f, 3.31e-2f),
            3.0f, Vec3f(3e-3f), Vec3f(3e-3f)/0.9f, 0.65f
        );
    }
};

class AtmosphericScattering {
    AtmosphereParameters params;

    static const int TransmittanceW = 256;
    static const int TransmittanceH = 64;
    static const int IrradianceW = 64;
    static const int IrradianceH = 16;
    static const int ScatterR   = 32;
    static const int ScatterMu  = 128;
    static const int ScatterMuS = 32;
    static const int ScatterNu  = 8;
    static const int ScatterElements = ScatterR*ScatterMu*ScatterMuS*ScatterNu;

    Vec3f *transmittanceTable;
    Vec3f *irradianceTable;
    Vec3f *scatterTable;

    Vec3f *deltaIrradiance;
    Vec3f *deltaMultiscatter;
    Vec3f *deltaInscatter;
    Vec3f *singleScatterRayleigh;
    Vec3f *singleScatterMie;

    float phaseRayleigh(float cosTheta) const;
    float phaseMie(float cosTheta) const;

    float rayLength(float r, float mu) const;
    float opticalDepth(float h, float r, float mu) const;
    float analyticOpticalDepth(float h, float r, float mu, float d) const;
    void singleScatterEvent(float r, float mu, float muS, float nu, float t, Vec3f &rayleigh, Vec3f &mie) const;
    Vec3f multiScatterEvent(float r, float mu, float muS, float nu, float t) const;
    void singleInscatter(float r, float mu, float muS, float nu, Vec3f &rayleigh, Vec3f &mie) const;
    Vec3f multiInscatter(float r, float mu, float muS, float nu) const;
    Vec3f inscatter(float r, float mu, float muS, float nu, bool fromSingle) const;
    Vec3f irradiance(float r, float muS, bool fromSingle) const;
    Vec3f transmittance(float dRayleight, float dMie) const;

    void warpTransmittance(float &r, float &mu) const;
    void unwarpTransmittance(float &r, float &mu) const;
    void warpIrradiance(float &r, float &mu) const;
    void unwarpIrradiance(float &r, float &mu) const;
    void warpScatter(float &r, float &mu, float &muS, float &nu) const;
    void unwarpScatter(float &r, float &mu, float &muS, float &nu) const;

    Vec3f atmosphereTransmittance(float r, float mu) const;
    Vec3f analyticTransmittance(float r, float mu, float d) const;
    Vec3f segmentTransmittance(float r, float mu, float d) const;

    void precomputeTransmittance();
    void precomputeInitialIrradiance();
    void precomputeInitialInscatter();
    void precomputeInscatter(bool fromSingle, int iter);
    void precomputeIrradiance(bool fromSingle, int iter);
    void precomputeMultiscatter(int iter);
    void addDeltas();
    void precomputeIterate();

    Vec3f reducedRadiance(float r, float mu, const Vec3f &v, const Vec3f &s, const Vec3f &c) const;
    Vec3f inscatteredRadiance(float r, float mu, float muS, float nu, float t, const Vec3f &o, const Vec3f &v, const Vec3f &s) const;
    Vec3f reflectedRadiance(const Vec3f &o, const Vec3f &v, const Vec3f &s, const Vec3f &n, const Vec3f &c, float t, float st, float ao) const;

public:
    void precompute();
    Vec3f inscatteredRadiance(const Vec3f &o, const Vec3f &v, const Vec3f &s) const;
    Vec3f evalSimple(const Vec3f &v, const Vec3f &s, const Vec3f &e) const;
    Vec3f evaluate(const Vec3f &o, const Vec3f &v, const Vec3f &s, const Vec3f &n, const Vec3f &c, float t, float st, float ao) const;

    AtmosphericScattering(const AtmosphereParameters &params_) :
        params(params_), transmittanceTable(0) {}
};

}

#endif /* ATMOSPHERICSCATTERING_HPP_ */
