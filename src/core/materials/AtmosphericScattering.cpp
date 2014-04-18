#include <tbb/parallel_for.h>
#include <algorithm>
#include <stdio.h>

#include "AtmosphericScattering.hpp"

#include "math/MathUtil.hpp"
#include "math/Angle.hpp"

namespace Tungsten
{

const float Rg = 6360.0f;
const float Rt = 6420.0f;
const float Rl = 6421.0f; /* TODO: Is this necessary? */

const float SunAngularDiameter = 4.0f*0.53f*PI/180.0f;

const float TransmittanceMuLower = -0.15f; // Lower bound for -sqrt(1.0 - Rg*Rg/(Rt*Rt))

const int TransmittanceSamples = 512;
const int MultiscatterSamples = 50;
const int IrradianceSamples = 32;
const int InscatterSamples = 16;

#define TRANSMITTANCE_NON_LINEAR 1
#define  MULTISCATTER_NON_LINEAR 1

Vec3f lerp(const Vec3f &a, const Vec3f &b, float t) {
    return a*(1.0f - t) + b*t;
}

Vec3f lerp(const Vec3f x[], float s, int w) {
    s *= w;

    int i = min((int)s, w - 2);
    float a = min(s - i, 1.0f);

    return lerp(x[i], x[i + 1], a);
}

Vec3f lerp(const Vec3f x[], float s, float t, int w, int h) {
    t *= h;

    int i = min((int)t, h - 2);
    float a = min(t - i, 1.0f);

    return lerp(
        lerp(x +       i*w, s, w),
        lerp(x + (i + 1)*w, s, w),
        a
    );
}

Vec3f lerp(const Vec3f x[], float s, float t, float u, int w, int h, int d) {
    u *= d;

    int i = min((int)u, d - 2);
    float a = min(u - i, 1.0f);

    return lerp(
        lerp(x +       i*w*h, s, t, w, h),
        lerp(x + (i + 1)*w*h, s, t, w, h),
        a
    );
}

Vec3f lerp(const Vec3f x[], float s, float t, float u, float v, int w, int h, int d, int g) {
    v *= g;

    int i = min((int)v, g - 2);
    float a = min(v - i, 1.0f);

    return lerp(
        lerp(x +       i*w*h*d, s, t, u, w, h, d),
        lerp(x + (i + 1)*w*h*d, s, t, u, w, h, d),
        a
    );
}

float AtmosphericScattering::phaseRayleigh(float cosTheta) const {
    return 3.0f/(16.0f*PI)*(1.0f + cosTheta*cosTheta);
}

float AtmosphericScattering::phaseMie(float cosTheta) const {
    float denom = (1.0f + params.mieGSq - 2.0f*params.mieG*cosTheta);
    denom *= denom*denom;
    denom = (2.0f + params.mieGSq)*std::sqrt(denom);

    return (3.0f/(8.0f*PI))*((1.0f - params.mieGSq)*(1.0f + cosTheta*cosTheta))/denom;
}

/* Length of ray before it hits the ground/outer atmosphere. r has to be within atmosphere */
float AtmosphericScattering::rayLength(float r, float mu) const {
    float discriminant = r*r*(mu*mu - 1.0) + Rg*Rg;

    if (discriminant >= 0.0f) {
        float tGround = -r*mu - std::sqrt(discriminant);

        if (tGround > 0.0f)
            return tGround;
    }

    return -r*mu + std::sqrt(r*r*(mu*mu - 1.0) + Rl*Rl); /* TODO: Replace with Rt safe? */
}

float AtmosphericScattering::opticalDepth(float h, float r, float mu) const {
    float t = rayLength(r, mu);
    float dx = t/TransmittanceSamples;
    float yOld = std::exp(-(r - Rg)/h);

    float depth = 0.0f;
    for (int i = 0; i < TransmittanceSamples; i++) {
        float ti = (i + 1)*dx;
        float height = std::sqrt(r*r + 2.0f*r*ti*mu + ti*ti);
        float yNew = std::exp(-(height - Rg)/h);

        depth += (yOld + yNew)*0.5f*dx; /* Trapezoid rule */

        yOld = yNew;
    }

    return depth;
}

/* See http://evasion.inrialpes.fr/~Eric.Bruneton/ */
/* I tried finding a derivation for this, but no luck.
 *
 * It is not mentioned in the paper and only exists in the code.
 * It seems to solve accuracy issues when using the transmittance
 * identity with the tabulated transmittance.
 *
 * Should probably benchmark the accuracy of this at
 * some point - I'm not even sure whether this is an
 * approximate fit or the exact solution.
 */
float AtmosphericScattering::analyticOpticalDepth(float h, float r, float mu, float d) const {
    float a = std::sqrt((0.5f/h)*r);

    float a0 = a*mu;
    float a1 = a*(mu + d/r);
    float a0s = sgn(a0);
    float a1s = sgn(a1);
    float a0sq = a0*a0;
    float a1sq = a1*a1;

    float x = a1s > a0s ? std::exp(a0sq) : 0.0f;

    float y0 = a0s/(2.3193f*std::abs(a0) + std::sqrt(1.52f*a0sq + 4.0f));
    float y1 = a1s/(2.3193f*std::abs(a1) + std::sqrt(1.52f*a1sq + 4.0f))*std::exp(-d/h*(d/(2.0f*r) + mu));

    return std::sqrt((6.2831f*h)*r)*std::exp((Rg - r)/h)*(x + y0 - y1);
}

Vec3f AtmosphericScattering::transmittance(float dRayleigh, float dMie) const {
    Vec3f rayleighDepth = params.rayleighSigmaS*dRayleigh;
    Vec3f      mieDepth = params.     mieSigmaT*dMie;

    return std::exp(-rayleighDepth - mieDepth);
}

void AtmosphericScattering::warpTransmittance(float &r, float &mu) const {
#if TRANSMITTANCE_NON_LINEAR
    r *= r;
    mu = std::tan(1.5f*mu)/std::tan(1.5f);
#endif

    r  = Rg + r*(Rt - Rg);
    mu = TransmittanceMuLower + mu*(1.0f - TransmittanceMuLower);
}

void AtmosphericScattering::warpIrradiance(float &r, float &mu) const {
    r  = Rg + r*(Rt - Rg);
    //mu = TransmittanceMuLower + mu*(1.0f - TransmittanceMuLower);  /* TODO: Necessary? */
    mu = -0.2f + mu*(1.0f + 0.2f);
}


void AtmosphericScattering::warpScatter(float &r, float &mu, float &muS, float &nu) const {
#if MULTISCATTER_NON_LINEAR
    float add = (r == 0.0f ? 0.01f : (r >= 0.999f ? -0.001f : 0.0f));
    r = std::sqrt(r*r*(Rt*Rt - Rg*Rg) + Rg*Rg) + add;

    //muS = -1.0f/3.0f*(logf(-muS*(1.0f - std::exp(-3.6f)) + 1.0f) + 0.6f);
    /* 'Better formula' from code that comes with the paper */
    muS = std::tan((2.0f*muS - 1.0f + 0.26f)*1.1f)/std::tan(1.26f*1.1f);

    const float H = std::sqrt((Rt*Rt - Rg*Rg));
    float rho = std::sqrt((r*r - Rg*Rg));

    /*if (mu < 0.5f) {
        mu = (mu - 0.5f)*2.0f*rho;
        //mu = min(max(rho, mu), rho*0.999f);
        mu = (mu*mu + r*r - Rg*Rg)/(2.0f*r*mu);
        mu = min(max(mu, -0.999f), 0.0f);
    } else {
        mu = (mu - 0.5f)*2.0f*(rho + H);
        //mu = min(max(Rt - r, mu), (rho + H)*0.999f);
        mu = -(mu*mu + r*r - Rt*Rt)/(2.0f*r*mu);
        mu = max(min(mu, 0.999f), 0.0f);
    }*/
    if (mu < 0.5f*ScatterMu) {
        float d = 1.0f - mu/(ScatterMu/2.0f - 1.0f);
        d = min(max(r - Rg, d*rho), rho*0.999f);
        mu = (Rg*Rg - r*r - d*d)/(2.0f*r*d);
        mu = min(mu, -std::sqrt(1.0f - (Rg/r)*(Rg/r)) - 0.001f);
    } else {
        float d = (mu - ScatterMu/2.0f)/(ScatterMu/2.0f - 1.0f);
        d = min(max(Rt - r, d*(rho + H)), (rho + H)*0.999f);
        mu = (Rt*Rt - r*r - d*d)/(2.0f*r*d);
    }
    mu = min(mu, 1.0f);

#else
    r = Rg + r*(Rt - Rg);
    //mu = TransmittanceMuLower + mu*(1.0f - TransmittanceMuLower);  /* TODO: Necessary? */
    mu  = -0.2f + mu *(1.0f + 0.2f);
    muS = -0.2f + muS*(1.0f + 0.2f);
#endif
    nu  = nu*2.0f - 1.0f;
}

void AtmosphericScattering::unwarpScatter(float &r, float &mu, float &muS, float &nu) const {
#if MULTISCATTER_NON_LINEAR
    const float H = std::sqrt((Rt*Rt - Rg*Rg));
    float rho = std::sqrt(max(r*r - Rg*Rg, 0.0f));
    float delta = r*r*mu*mu - rho*rho;

    if (r*mu < 0.0f && delta > 0.0f)
        mu = (rho == 0.0f ? 0.5f : 0.5f + (r*mu + std::sqrt(delta))/(2.0f*rho));
    else
        mu = 0.5f - (r*mu - std::sqrt(delta + H*H))/(2.0f*rho + 2.0f*H);
    r = rho/H;

    //muS = (1.0f - std::exp(-3.0*muS - 0.6f))/(1.0f - std::exp(-3.6f));
    /* 'Better formula' from code that comes with the paper */
    muS = 0.5f/ScatterMuS + (std::atan(max(muS, -0.1975f)*std::tan(1.26f*1.1f))/1.1f + (1.0f - 0.26f))*0.5f*(1.0f - 1.0f/ScatterMuS);
    muS = max(0.0f, muS);
#else
    r  = (r - Rg)/(Rt - Rg);
    //mu = (mu - TransmittanceMuLower)/(1.0f - TransmittanceMuLower); /* TODO: Necessary? */
    mu  = (mu  + 0.2f)/(1.0f + 0.2f);
    muS = (muS + 0.2f)/(1.0f + 0.2f);
#endif
    nu  = nu*0.5f + 0.5f;
}

void AtmosphericScattering::unwarpTransmittance(float &r, float &mu) const {
    r  = (r - Rg)/(Rt - Rg);
    mu = (mu - TransmittanceMuLower)/(1.0f - TransmittanceMuLower);

#if TRANSMITTANCE_NON_LINEAR
    r  = std::sqrt(max(r, 0.0f));
    mu = std::atan(mu*std::tan(1.5f))/1.5f;
#endif

    mu = min(max(0.0f, mu), 1.0f);
}

void AtmosphericScattering::unwarpIrradiance(float &r, float &mu) const {
    r  = (r - Rg)/(Rt - Rg);
    //mu = (mu - TransmittanceMuLower)/(1.0f - TransmittanceMuLower); /* TODO: Necessary? */
    mu = (mu + 0.2f)/(1.0f + 0.2f);
}

void AtmosphericScattering::precomputeTransmittance() {
    transmittanceTable = new Vec3f[TransmittanceW*TransmittanceH];

    tbb::parallel_for(0, TransmittanceW*TransmittanceH, [&](size_t idx) {
        int x = idx % TransmittanceW;
        int y = idx / TransmittanceW;

        float r  = y/(TransmittanceH - 1.0f);
        float mu = x/(TransmittanceW - 1.0f);

        warpTransmittance(r, mu);

        if (mu < -std::sqrt(1.0f - Rg*Rg/(r*r)))
            transmittanceTable[idx] = Vec3f(0.0f);
        else
            transmittanceTable[idx] = transmittance(
                opticalDepth(params.rayleighH, r, mu),
                opticalDepth(params.     mieH, r, mu)
            );
    });
}

void AtmosphericScattering::precomputeInitialIrradiance() {
    deltaIrradiance = new Vec3f[IrradianceW*IrradianceH];

    tbb::parallel_for(0, IrradianceW*IrradianceH, [&](size_t idx) {
        int x = idx % IrradianceW;
        int y = idx / IrradianceW;

        float r  = y/(IrradianceH - 1.0f);
        float mu = x/(IrradianceW - 1.0f);

        warpIrradiance(r, mu);

        deltaIrradiance[idx] = atmosphereTransmittance(r, mu)*max(mu, 0.0f);
    });
}

void AtmosphericScattering::singleScatterEvent(float r, float mu, float muS, float nu, float t, Vec3f &rayleigh, Vec3f &mie) const {
    float height = std::sqrt(r*r + 2.0f*r*mu*t + t*t);
    float muSi = (nu*t + muS*r)/height; /* TODO: Find derivation for this */
    height = max(height, Rg);

    if (muSi >= -std::sqrt(1.0f - Rg*Rg/(height*height))) {
        Vec3f T = segmentTransmittance(r, mu, t)*atmosphereTransmittance(height, muSi);

        rayleigh = T*std::exp(-(height - Rg)/params.rayleighH);
        mie      = T*std::exp(-(height - Rg)/params.     mieH);
    } else {
        rayleigh = Vec3f(0.0f);
        mie      = Vec3f(0.0f);
    }
}

Vec3f AtmosphericScattering::multiScatterEvent(float r, float mu, float muS, float nu, float t) const {
    float height = std::sqrt(r*r + 2.0f*r*mu*t + t*t);
    float mui = (r*mu + t)/height;
    float muSi = (nu*t + muS*r)/height;
    height = max(height, Rg);

    unwarpScatter(height, mui, muSi, nu);

    return segmentTransmittance(r, mu, t)*lerp(deltaInscatter, height, mui, muSi, nu, ScatterR, ScatterMu, ScatterMuS, ScatterNu);
}

void AtmosphericScattering::singleInscatter(float r, float mu, float muS, float nu, Vec3f &rayleigh, Vec3f &mie) const {
    rayleigh = Vec3f(0.0f);
    mie      = Vec3f(0.0f);

    float t = rayLength(r, mu);
    float dx = t/MultiscatterSamples;

    Vec3f rayleighOld, mieOld;
    singleScatterEvent(r, mu, muS, nu, 0.0f, rayleighOld, mieOld);
    for (int i = 0; i < MultiscatterSamples; i++) {
        float ti = (i + 1)*dx;

        Vec3f rayleighNew, mieNew;
        singleScatterEvent(r, mu, muS, nu, ti, rayleighNew, mieNew);

        rayleigh += (rayleighOld + rayleighNew)*0.5*dx;
        mie      += (     mieOld +      mieNew)*0.5*dx;

        rayleighOld = rayleighNew;
             mieOld =      mieNew;
    }

    rayleigh *= params.rayleighSigmaS;
    mie      *= params.mieSigmaS;
}

Vec3f AtmosphericScattering::multiInscatter(float r, float mu, float muS, float nu) const {
    Vec3f raymie(0.0f);

    float t = rayLength(r, mu);
    float dx = t/MultiscatterSamples;

    Vec3f raymieOld = multiScatterEvent(r, mu, muS, nu, 0.0f);
    for (int i = 0; i < MultiscatterSamples; i++) {
        float ti = (i + 1)*dx;
        ti = min(ti, t*0.999f);

        Vec3f raymieNew = multiScatterEvent(r, mu, muS, nu, ti);
        raymie += (raymieOld + raymieNew)*0.5*dx;
        raymieOld = raymieNew;
    }

    return raymie;
}

void saveBytes(const char *path, void *data, size_t size, size_t length) {
    FILE *fp = fopen(path, "wb");

    fwrite(data, size, length, fp);

    fclose(fp);
}

bool loadBytes(const char *path, void *data, size_t size, size_t length) {
    FILE *fp = fopen(path, "rb");

    if (!fp)
        return false;

    fread(data, size, length, fp);

    fclose(fp);

    return true;
}

void AtmosphericScattering::precomputeInitialInscatter() {
    singleScatterRayleigh = new Vec3f[ScatterElements];
    singleScatterMie      = new Vec3f[ScatterElements];

    if (loadBytes("singleScatterRayleigh.dat", singleScatterRayleigh, sizeof(Vec3f), ScatterElements) &&
        loadBytes("singleScatterMie.dat",      singleScatterMie,      sizeof(Vec3f), ScatterElements))
        return;

    tbb::parallel_for(0, ScatterNu*ScatterMuS*ScatterMu*ScatterR, [&](size_t idx) {
        int x =  idx                       % (ScatterR);
        int y = (idx/(ScatterR))           % (ScatterMu);
        int z = (idx/(ScatterR*ScatterMu)) % (ScatterMuS);
        int w = (idx/(ScatterR*ScatterMu*ScatterMuS));

        float nu  = w/(ScatterNu  - 1.0f);
        float muS = z/(ScatterMuS - 1.0f);
        float mu  = y;///(ScatterMu  - 1.0f);
        float r   = x/(ScatterR   - 1.0f);

        warpScatter(r, mu, muS, nu);

        singleInscatter(r, mu, muS, nu, singleScatterRayleigh[idx], singleScatterMie[idx]);
    });

    saveBytes("singleScatterRayleigh.dat", singleScatterRayleigh, sizeof(Vec3f), ScatterElements);
    saveBytes("singleScatterMie.dat",      singleScatterMie,      sizeof(Vec3f), ScatterElements);

}

Vec3f AtmosphericScattering::inscatter(float r, float mu, float muS, float nu, bool fromSingle) const {
    const float dTheta = PI/InscatterSamples;
    const float dPhi   = PI/InscatterSamples;

    r = clamp(r, Rg, Rt);
    mu = clamp(mu, -1.0f, 1.0f);
    muS = clamp(muS, -1.0f, 1.0f);
    float var = sqrt(1.0f - mu*mu) * sqrt(1.0f - muS*muS);
    nu = clamp(nu, muS*mu - var, muS*mu + var);

    float thetaGround = -std::sqrt(1.0f - Rg*Rg/(r*r));

    Vec3f v(std::sqrt(1.0f - mu*mu), mu, 0.0f);

    Vec3f s;
    if (std::abs(v.x()) < 1e-3f)
        s = Vec3f(0.0f, muS, std::sqrt(1.0f - muS*muS));
    else {
        float x = (nu - mu*muS)/v.x();
        s = Vec3f(x, muS, std::sqrt(max(0.0f, 1.0f - x*x - muS*muS))); /* TODO: Wat. This thing isn't even normalized half the time */
    }

    Vec3f raymie = Vec3f(0.0f);

    for (int thetaI = 0; thetaI < InscatterSamples; thetaI++) {
        float theta = (thetaI + 0.5f)*dTheta;
        float cosTheta = std::cos(theta);

        Vec3f trans;
        float tGround;
        if (cosTheta < thetaGround) {
            tGround = -r*cosTheta - std::sqrt(r*r*(cosTheta*cosTheta - 1.0f) + Rg*Rg);
            //trans = segmentTransmittance(r, cosTheta, tGround);
            trans = segmentTransmittance(Rg, -(r*cosTheta + tGround)/Rg, tGround);
        }

        for (int phiI = 0; phiI < InscatterSamples*2; phiI++) {
            float phi = (phiI + 0.5f)*dPhi;

            float dW = dTheta*dPhi*std::sin(theta); /* Solid angle */

            Vec3f w(std::sin(theta)*std::cos(phi), cosTheta, std::sin(theta)*std::sin(phi));

            float cosThetaN = v.dot(w);
            float cosThetaM = s.dot(w);
            float pRayleigh = phaseRayleigh(cosThetaN);
            float pMie      = phaseMie     (cosThetaN);

            float rP = r, muP = cosTheta, muSP = muS, nuP = cosThetaM;
            unwarpScatter(rP, muP, muSP, nuP);

            Vec3f contribution(0.0f);

            if (cosTheta < thetaGround) {
                Vec3f normal = (Vec3f(0.0f, r, 0.0f) + w*tGround)/Rg;
                float rV = Rg, muU = normal.dot(s);
                unwarpIrradiance(rV, muU);
                /* TODO: Reflectance value */
                contribution += lerp(deltaIrradiance, muU, rV, IrradianceW, IrradianceH)*trans*(0.1f/PI);
            }

            if (fromSingle) {
                float pRayleighS = phaseRayleigh(cosThetaM);
                float pMieS      = phaseMie     (cosThetaM);

                Vec3f ssRayleigh = lerp(singleScatterRayleigh, rP, muP, muSP, nuP, ScatterR, ScatterMu, ScatterMuS, ScatterNu);
                Vec3f ssMie      = lerp(singleScatterMie,      rP, muP, muSP, nuP, ScatterR, ScatterMu, ScatterMuS, ScatterNu);

                contribution += ssRayleigh*pRayleighS + ssMie*pMieS;
            } else
                contribution += lerp(deltaMultiscatter, rP, muP, muSP, nuP, ScatterR, ScatterMu, ScatterMuS, ScatterNu);

            Vec3f SigmaSR = params.rayleighSigmaS*std::exp(-(r - Rg)/params.rayleighH);
            Vec3f SigmaSM = params.mieSigmaS     *std::exp(-(r - Rg)/params.mieH);

            raymie += contribution*(SigmaSR*pRayleigh + SigmaSM*pMie)*dW;
        }
    }

    return raymie;
}

Vec3f AtmosphericScattering::irradiance(float r, float muS, bool fromSingle) const {
    const float dTheta = PI/InscatterSamples;
    const float dPhi   = PI/InscatterSamples;

    Vec3f s(std::sqrt(1.0f - muS*muS), muS, 0.0f);

    Vec3f irradiance(0.0f);
    for (int thetaI = 0; thetaI < InscatterSamples/2; thetaI++) {
        float theta = (thetaI + 0.5f)*dTheta;
        float cosTheta = std::cos(theta);

        for (int phiI = 0; phiI < InscatterSamples*2; phiI++) {
            float phi = (phiI + 0.5f)*dPhi;

            float dW = dTheta*dPhi*std::sin(theta); /* Solid angle */

            Vec3f w(std::sin(theta)*std::cos(phi), cosTheta, std::sin(theta)*std::sin(phi));

            float rP = r, muP = cosTheta, muSP = muS, nuP = w.dot(s);
            unwarpScatter(rP, muP, muSP, nuP);

            Vec3f contribution;
            if (fromSingle) {
                float cosThetaM = s.dot(w);
                float pRayleighS = phaseRayleigh(cosThetaM);
                float pMieS      = phaseMie     (cosThetaM);

                Vec3f ssRayleigh = lerp(singleScatterRayleigh, rP, muP, muSP, nuP, ScatterR, ScatterMu, ScatterMuS, ScatterNu);
                Vec3f ssMie      = lerp(singleScatterMie,      rP, muP, muSP, nuP, ScatterR, ScatterMu, ScatterMuS, ScatterNu);

                contribution = ssRayleigh*pRayleighS + ssMie*pMieS;
            } else
                contribution = lerp(deltaMultiscatter, rP, muP, muSP, nuP, ScatterR, ScatterMu, ScatterMuS, ScatterNu);

            irradiance += contribution*cosTheta*dW;
        }
    }

    return irradiance;
}

void AtmosphericScattering::precomputeInscatter(bool fromSingle, int iter) {
    char path[256];
    sprintf(path, "inscatter-%d.dat", iter);

    if (loadBytes(path, deltaInscatter, sizeof(Vec3f), ScatterElements))
        return;

    tbb::parallel_for(0, ScatterNu*ScatterMuS*ScatterMu*ScatterR, [&](size_t idx) {
        int x =  idx                       % (ScatterR);
        int y = (idx/(ScatterR))           % (ScatterMu);
        int z = (idx/(ScatterR*ScatterMu)) % (ScatterMuS);
        int w = (idx/(ScatterR*ScatterMu*ScatterMuS));

        float nu  = w/(ScatterNu  - 1.0f);
        float muS = z/(ScatterMuS - 1.0f);
        float mu  = y;///(ScatterMu  - 1.0f);
        float r   = x/(ScatterR   - 1.0f);

        warpScatter(r, mu, muS, nu);

        deltaInscatter[idx] = inscatter(r, mu, muS, nu, fromSingle);
    });

    saveBytes(path, deltaInscatter, sizeof(Vec3f), ScatterElements);
}

void AtmosphericScattering::precomputeIrradiance(bool fromSingle, int /*iter*/) {
    tbb::parallel_for(0, IrradianceW*IrradianceH, [&](size_t idx) {
        int x = idx % IrradianceW;
        int y = idx / IrradianceW;

        float r   = y/(IrradianceH - 1.0f);
        float muS = x/(IrradianceW - 1.0f);

        warpIrradiance(r, muS);

        deltaIrradiance[idx] = irradiance(r, muS, fromSingle);
    });
}

void AtmosphericScattering::precomputeMultiscatter(int iter) {
    char path[256];
    sprintf(path, "multiscatter-%d.dat", iter);

    if (loadBytes(path, deltaMultiscatter, sizeof(Vec3f), ScatterElements))
        return;

    tbb::parallel_for(0, ScatterNu*ScatterMuS*ScatterMu*ScatterR, [&](size_t idx) {
        int x =  idx                       % (ScatterR);
        int y = (idx/(ScatterR))           % (ScatterMu);
        int z = (idx/(ScatterR*ScatterMu)) % (ScatterMuS);
        int w = (idx/(ScatterR*ScatterMu*ScatterMuS));

        float nu  = w/(ScatterNu  - 1.0f);
        float muS = z/(ScatterMuS - 1.0f);
        float mu  = y;///(ScatterMu  - 1.0f);
        float r   = x/(ScatterR   - 1.0f);

        warpScatter(r, mu, muS, nu);

        deltaMultiscatter[idx] = multiInscatter(r, mu, muS, nu);
    });

    saveBytes(path, deltaMultiscatter, sizeof(Vec3f), ScatterElements);
}

void AtmosphericScattering::addDeltas() {
    for (int i = 0; i < IrradianceW*IrradianceH; i++)
        irradianceTable[i] += deltaIrradiance[i];

    int idx = 0;
    for (int w = 0; w < ScatterNu; w++) {
        for (int z = 0; z < ScatterMuS; z++) {
            for (int y = 0; y < ScatterMu; y++) {
                for (int x = 0; x < ScatterR; x++, idx++) {
                    float nu  = w/(ScatterNu  - 1.0f);
                    float muS = z/(ScatterMuS - 1.0f);
                    float mu  = y;///(ScatterMu  - 1.0f);
                    float r   = x/(ScatterR   - 1.0f);

                    warpScatter(r, mu, muS, nu);

                    scatterTable[idx] += deltaMultiscatter[idx]/phaseRayleigh(nu);
                }
            }
        }
    }
}

void AtmosphericScattering::precomputeIterate() {
    irradianceTable   = new Vec3f[IrradianceW*IrradianceH];

    scatterTable      = new Vec3f[ScatterElements];
    deltaMultiscatter = new Vec3f[ScatterElements];
    deltaInscatter    = new Vec3f[ScatterElements];

    for (int i = 0; i < ScatterElements; i++)
        scatterTable[i] = singleScatterRayleigh[i];

    for (int i = 2; i <= 4; i++) {
        precomputeInscatter(i == 2, i);
        precomputeIrradiance(i == 2, i);
        precomputeMultiscatter(i);
        addDeltas();

        printf("Iteration %d completed!\n", i);
        fflush(stdout);
    }
}

Vec3f AtmosphericScattering::atmosphereTransmittance(float r, float mu) const {
    unwarpTransmittance(r, mu);

    return lerp(transmittanceTable, mu, r, TransmittanceW, TransmittanceH);
}

Vec3f AtmosphericScattering::analyticTransmittance(float r, float mu, float d) const {
    return transmittance(
        analyticOpticalDepth(params.rayleighH, r, mu, d),
        analyticOpticalDepth(params.     mieH, r, mu, d)
    );
}

Vec3f AtmosphericScattering::segmentTransmittance(float r, float mu, float d) const {
    float r1  = std::sqrt(r*r + 2.0f*r*mu*d + d*d);
    float mu1 = (r*mu + d)/r1;

    if (mu > 0.0f)
        return min((atmosphereTransmittance(r, mu)/atmosphereTransmittance(r1, mu1)), Vec3f(1.0f));
    else
        return min((atmosphereTransmittance(r1, -mu1)/atmosphereTransmittance(r, -mu)), Vec3f(1.0f));
}

void AtmosphericScattering::precompute() {
    precomputeTransmittance();
    precomputeInitialIrradiance();
    precomputeInitialInscatter();
    precomputeIterate();
}

Vec3f AtmosphericScattering::reducedRadiance(float r, float mu, const Vec3f &v, const Vec3f &s, const Vec3f &c) const {
    const float sunCos = std::cos(SunAngularDiameter);

    float d = v.dot(s);

    if (d > sunCos) {
        float rs = (1.0f - d)/(1.0f - sunCos);
        return atmosphereTransmittance(r, mu)*c*std::sqrt(1.0 - rs*rs)*smoothStep(sunCos, 1.0f, d);
    } else
        return Vec3f(0.0f);
}

Vec3f AtmosphericScattering::inscatteredRadiance(float r, float mu, float muS, float nu, float t, const Vec3f &o, const Vec3f &v, const Vec3f &s) const {
    float nu0 = nu;

    float pRayleigh = phaseRayleigh(nu);
    float pMie      = phaseMie     (nu);

    float tmpR = r, tmpMu = mu, tmpMuS = muS, tmpNu = nu;
    unwarpScatter(tmpR, tmpMu, tmpMuS, tmpNu);

    Vec3f ssRayleigh = lerp(scatterTable,     tmpR, tmpMu, tmpMuS, tmpNu, ScatterR, ScatterMu, ScatterMuS, ScatterNu);
    Vec3f ssMie      = lerp(singleScatterMie, tmpR, tmpMu, tmpMuS, tmpNu, ScatterR, ScatterMu, ScatterMuS, ScatterNu);

    if (t > 0.0f) {
        Vec3f x0 = o + v*t;
        x0.y() = max(x0.y(), 0.0f) + Rg;
        float r0 = x0.length();
        float mu0 = x0.dot(v)/r0;
        float muS0 = x0.dot(s)/r0;

        if (r0 > Rg + 0.01f) {
            unwarpScatter(r0, mu0, muS0, nu0);

            Vec3f ssRayleigh0 = lerp(scatterTable,     r0, mu0, muS0, nu0, ScatterR, ScatterMu, ScatterMuS, ScatterNu);
            Vec3f ssMie0      = lerp(singleScatterMie, r0, mu0, muS0, nu0, ScatterR, ScatterMu, ScatterMuS, ScatterNu);

            Vec3f trans = analyticTransmittance(r, mu, t);
            ssRayleigh -= trans*ssRayleigh0;
            ssMie      -= trans*ssMie0;

            ssRayleigh = max(ssRayleigh, Vec3f(0.0f));
            ssMie      = max(ssMie, Vec3f(0.0f));
        }
    }

    return ssRayleigh*pRayleigh + ssMie*pMie;
}

Vec3f AtmosphericScattering::reflectedRadiance(const Vec3f &o, const Vec3f &v, const Vec3f &s, const Vec3f &n, const Vec3f &c, float t, float st, float ao) const {
    Vec3f x0 = o + v*t;
    x0.y() += Rg;
    float r0 = x0.length();
    float r   = o.y() + Rg;
    float mu  = v.y();
    float muS = s.y();

    float tmpR = r0, tmpMuS = muS;
    unwarpIrradiance(tmpR, tmpMuS);

    Vec3f irradiance  = lerp(irradianceTable, tmpMuS, tmpR, IrradianceW, IrradianceH);
    Vec3f reflectance = (st < t ? Vec3f(0.0f) : atmosphereTransmittance(r0, muS)*max(n.dot(s), 0.0f));

    return analyticTransmittance(r, mu, t)*(irradiance + reflectance)*c*(1.0 - ao);
}

Vec3f AtmosphericScattering::inscatteredRadiance(const Vec3f &o, const Vec3f &v, const Vec3f &s) const
{
    float r   = max(o.y(), 0.001f) + Rg;
    float mu  = v.y();
    float muS = s.y();
    float nu  = v.dot(s);

    return clamp(inscatteredRadiance(r, mu, muS, nu, rayLength(r, mu), o, v, s), Vec3f(0.0f), Vec3f(1.0f));
}

Vec3f AtmosphericScattering::evalSimple(const Vec3f &v, const Vec3f &s, const Vec3f &/*e*/) const
{
    float r   = Rg + 0.001f;
    float mu  = v.y();
    float muS = s.y();
    float nu  = v.dot(s);

    return inscatteredRadiance(r, mu, muS, nu, rayLength(r, mu), Vec3f(0.0f), v, s);
         //+ reducedRadiance(r, mu, v, s, e);
}

Vec3f AtmosphericScattering::evaluate(const Vec3f &o, const Vec3f &v, const Vec3f &s, const Vec3f &n, const Vec3f &c, float t, float st, float ao) const {
    float r   = o.y() + Rg;
    float mu  = v.y();
    float muS = s.y();
    float nu  = v.dot(s);

    Vec3f radiance = inscatteredRadiance(r, mu, muS, nu, min(st, t), o, v, s);

    if (t < 0.0)
        radiance += reducedRadiance(r, mu, v, s, c);
    else
        radiance += reflectedRadiance(o, v, s, n, c, t, st, ao);

    return radiance;
}

}
