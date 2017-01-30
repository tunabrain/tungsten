#include "HairBcsdf.hpp"

#include "sampling/PathSampleGenerator.hpp"

#include "bsdfs/Fresnel.hpp"

#include "math/GaussLegendre.hpp"

#include "io/JsonObject.hpp"

namespace Tungsten {

HairBcsdf::HairBcsdf()
: _scaleAngleDeg(2.0f),
  _melaninRatio(0.5f),
  _melaninConcentration(0.25f),
  _overridesSigmaA(false),
  _sigmaA(0.0f),
  _roughness(0.1f)
{
    _lobes = BsdfLobes(BsdfLobes::GlossyLobe | BsdfLobes::AnisotropicLobe);
}

// Modified Bessel function of the first kind
float HairBcsdf::I0(float x)
{
    float result = 1.0f;
    float xSq = x*x;
    float xi = xSq;
    float denom = 4.0f;
    for (int i = 1; i <= 10; ++i) {
        result += xi/denom;
        xi *= xSq;
        denom *= 4.0f*float((i + 1)*(i + 1));
    }
    return result;
}

float HairBcsdf::logI0(float x)
{
    if (x > 12.0f)
        // More stable evaluation of log(I0(x))
        // See also https://publons.com/discussion/12/
        return x + 0.5f*(std::log(1.0f/(TWO_PI*x)) + 1.0f/(8.0f*x));
    else
        return std::log(I0(x));
}

// Standard normalized Gaussian
float HairBcsdf::g(float beta, float theta)
{
    return std::exp(-theta*theta/(2.0f*beta*beta))/(std::sqrt(2.0f*PI)*beta);
}

// Wrapped Gaussian "detector", computed as an infinite sum of Gaussians
// Approximated with a finite sum for obvious reasons.
// Note: This is merely to reproduce the paper. You could
// (and probably should) replace this with some other Gaussian-like
// function that can be analytically normalized and sampled
// over the [-Pi, Pi] domain. The Gaussian cannot, hence this
// slightly awkward and expensive evaluation.
float HairBcsdf::D(float beta, float phi)
{
    float result = 0.0f;
    float delta;
    float shift = 0.0f;
    do {
        delta = g(beta, phi + shift) + g(beta, phi - shift - TWO_PI);
        result += delta;
        shift += TWO_PI;
    } while (delta > 1e-4f);
    return result;
}

// Computes the exitant azimuthal angle of the p'th perfect specular
// scattering event, derived using Bravais theory
// See the paper "Light Scattering from Human Hair Fibers" for details
float HairBcsdf::Phi(float gammaI, float gammaT, int p)
{
    return 2.0f*p*gammaT - 2.0f*gammaI + p*PI;
}

// The following two functions are the guts of the azimuthal scattering function,
// following the paper. They are only provided for reference - the actual
// runtime evaluation uses precomputed versions of these functions
// sampled into a 2D table. Additionally, the precomputation turns these functions inside out
// to cache values that are constant across successive evaluations, and does not
// use these functons directly.

// Special case for the R lobe
float HairBcsdf::NrIntegrand(float beta, float halfWiDotWo, float phi, float h) const
{
    float gammaI = std::asin(clamp(h, -1.0f, 1.0f));
    float deltaPhi = phi + 2.0f*gammaI;
    deltaPhi = std::fmod(deltaPhi, TWO_PI);
    if (deltaPhi < 0.0f)
        deltaPhi += TWO_PI;

    return D(beta, deltaPhi)*Fresnel::dielectricReflectance(1.0f/Eta, halfWiDotWo);
}

Vec3f HairBcsdf::NpIntegrand(float beta, float cosThetaD, float phi, int p, float h) const
{
    float iorPrime = std::sqrt(Eta*Eta - (1.0f - cosThetaD*cosThetaD))/cosThetaD;
    float cosThetaT = std::sqrt(1.0f - (1.0f - cosThetaD*cosThetaD)*sqr(1.0f/Eta));
    Vec3f sigmaAPrime = _sigmaA/cosThetaT;

    float gammaI = std::asin(clamp(h, -1.0f, 1.0f));
    float gammaT = std::asin(clamp(h/iorPrime, -1.0f, 1.0f));
    // The correct internal path length (the one in d'Eon et al.'s paper
    // as well as Marschner et al.'s paper is wrong).
    // The correct factor is also mentioned in "Light Scattering from Filaments", eq. (20)
    float l = 2.0f*std::cos(gammaT);

    float f = Fresnel::dielectricReflectance(1.0f/Eta, cosThetaD*trigInverse(h));
    Vec3f T = std::exp(-sigmaAPrime*l);
    Vec3f Aph = (1.0f - f)*(1.0f - f)*T;
    for (int i = 1; i < p; ++i)
        Aph *= f*T;

    float deltaPhi = phi - Phi(gammaI, gammaT, p);
    deltaPhi = std::fmod(deltaPhi, TWO_PI);
    if (deltaPhi < 0.0f)
        deltaPhi += TWO_PI;

    return Aph*D(beta, deltaPhi);
}

// Rough longitudinal scattering function with variance v = beta^2
float HairBcsdf::M(float v, float sinThetaI, float sinThetaO, float cosThetaI, float cosThetaO) const
{
    float a = cosThetaI*cosThetaO/v;
    float b = sinThetaI*sinThetaO/v;

    if (v < 0.1f)
        // More numerically stable evaluation for small roughnesses
        // See https://publons.com/discussion/12/
        return std::exp(-b + logI0(a) - 1.0f/v + 0.6931f + std::log(1.0f/(2.0f*v)));
    else
        return std::exp(-b)*I0(a)/(2.0f*v*std::sinh(1.0f/v));
}

// Returns sinThetaO
float HairBcsdf::sampleM(float v, float sinThetaI, float cosThetaI, float xi1, float xi2) const
{
    // Version from the paper (very unstable)
    //float cosTheta = v*std::log(std::exp(1.0f/v) - 2.0f*xi1*std::sinh(1.0f/v));
    // More stable version from "Numerically stable sampling of the von Mises Fisher distribution on S2 (and other tricks)"
    float cosTheta = 1.0f + v*std::log(xi1 + (1.0f - xi1)*std::exp(-2.0f/v));
    float sinTheta = trigInverse(cosTheta);
    float cosPhi = std::cos(TWO_PI*xi2);

    return -cosTheta*sinThetaI + sinTheta*cosPhi*cosThetaI;
}

void HairBcsdf::fromJson(JsonPtr value, const Scene &scene)
{
    Bsdf::fromJson(value, scene);
    value.getField("scale_angle", _scaleAngleDeg);
    value.getField("melanin_ratio", _melaninRatio);
    value.getField("melanin_concentration", _melaninConcentration);
    _overridesSigmaA = value.getField("sigma_a", _sigmaA);
    value.getField("roughness", _roughness);
}

rapidjson::Value HairBcsdf::toJson(Allocator &allocator) const
{
    JsonObject result{Bsdf::toJson(allocator), allocator,
        "type", "hair",
        "scale_angle", _scaleAngleDeg,
        "roughness", _roughness
    };

    if (_overridesSigmaA)
        result.add("sigma_a", _sigmaA);
    else
        result.add("melanin_ratio", _melaninRatio,
                   "melanin_concentration", _melaninConcentration);

    return result;
}

Vec3f HairBcsdf::eval(const SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::GlossyLobe))
        return Vec3f(0.0f);

    float sinThetaI = event.wi.y();
    float sinThetaO = event.wo.y();
    float cosThetaO = trigInverse(sinThetaO);
    float thetaI = std::asin(clamp(sinThetaI, -1.0f, 1.0f));
    float thetaO = std::asin(clamp(sinThetaO, -1.0f, 1.0f));
    float thetaD = (thetaO - thetaI)*0.5f;
    float cosThetaD = std::cos(thetaD);

    float phi = std::atan2(event.wo.x(), event.wo.z());
    if (phi < 0.0f)
        phi += TWO_PI;

    // Lobe shift due to hair scale tilt, following the values in
    // "Importance Sampling for Physically-Based Hair Fiber Models"
    // rather than the earlier paper by Marschner et al. I believe
    // these are slightly more accurate.
    float thetaIR   = thetaI - 2.0f*_scaleAngleRad;
    float thetaITT  = thetaI +      _scaleAngleRad;
    float thetaITRT = thetaI + 4.0f*_scaleAngleRad;

    // Evaluate longitudinal scattering functions
    float MR   = M(_vR,   std::sin(thetaIR),   sinThetaO, std::cos(thetaIR),   cosThetaO);
    float MTT  = M(_vTT,  std::sin(thetaITT),  sinThetaO, std::cos(thetaITT),  cosThetaO);
    float MTRT = M(_vTRT, std::sin(thetaITRT), sinThetaO, std::cos(thetaITRT), cosThetaO);

    return   MR*  _nR->eval(phi, cosThetaD)
         +  MTT* _nTT->eval(phi, cosThetaD)
         + MTRT*_nTRT->eval(phi, cosThetaD);
}

bool HairBcsdf::sample(SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::GlossyLobe))
        return false;

    Vec2f xiN = event.sampler->next2D();
    Vec2f xiM = event.sampler->next2D();

    float sinThetaI = event.wi.y();
    float cosThetaI = trigInverse(sinThetaI);
    float thetaI = std::asin(clamp(sinThetaI, -1.0f, 1.0f));

    float thetaIR   = thetaI - 2.0f*_scaleAngleRad;
    float thetaITT  = thetaI +      _scaleAngleRad;
    float thetaITRT = thetaI + 4.0f*_scaleAngleRad;

    // The following lines are just lobe selection
    float weightR   = _nR  ->weight(cosThetaI);
    float weightTT  = _nTT ->weight(cosThetaI);
    float weightTRT = _nTRT->weight(cosThetaI);

    const PrecomputedAzimuthalLobe *lobe;
    float v;
    float theta;

    float target = xiN.x()*(weightR + weightTT + weightTRT);
    if (target < weightR) {
        v = _vR;
        theta = thetaIR;
        lobe = _nR.get();
    } else if (target < weightR + weightTT) {
        v = _vTT;
        theta = thetaITT;
        lobe = _nTT.get();
    } else {
        v = _vTRT;
        theta = thetaITRT;
        lobe = _nTRT.get();
    }

    // Actual sampling of the direction starts here
    float sinThetaO = sampleM(v, std::sin(theta), std::cos(theta), xiM.x(), xiM.y());
    float cosThetaO = trigInverse(sinThetaO);

    float thetaO = std::asin(clamp(sinThetaO, -1.0f, 1.0f));
    float thetaD = (thetaO - thetaI)*0.5f;
    float cosThetaD = std::cos(thetaD);

    float phi, phiPdf;
    lobe->sample(cosThetaD, xiN.y(), phi, phiPdf);

    float sinPhi = std::sin(phi);
    float cosPhi = std::cos(phi);

    event.wo = Vec3f(sinPhi*cosThetaO, sinThetaO, cosPhi*cosThetaO);
    event.pdf = pdf(event);
    event.weight = eval(event)/event.pdf;
    event.sampledLobe = BsdfLobes::GlossyLobe;

    return true;
}

float HairBcsdf::pdf(const SurfaceScatterEvent &event) const
{
    if (!event.requestedLobe.test(BsdfLobes::GlossyLobe))
        return 0.0f;

    float sinThetaI = event.wi.y();
    float sinThetaO = event.wo.y();
    float cosThetaI = trigInverse(sinThetaI);
    float cosThetaO = trigInverse(sinThetaO);
    float thetaI = std::asin(clamp(sinThetaI, -1.0f, 1.0f));
    float thetaO = std::asin(clamp(sinThetaO, -1.0f, 1.0f));
    float thetaD = (thetaO - thetaI)*0.5f;
    float cosThetaD = std::cos(thetaD);

    float phi = std::atan2(event.wo.x(), event.wo.z());
    if (phi < 0.0f)
        phi += TWO_PI;

    float thetaIR   = thetaI - 2.0f*_scaleAngleRad;
    float thetaITT  = thetaI +      _scaleAngleRad;
    float thetaITRT = thetaI + 4.0f*_scaleAngleRad;

    float weightR   = _nR  ->weight(cosThetaI);
    float weightTT  = _nTT ->weight(cosThetaI);
    float weightTRT = _nTRT->weight(cosThetaI);
    float weightSum = weightR + weightTT + weightTRT;

    float pdfR   = weightR  *M(_vR,   std::sin(thetaIR),   sinThetaO, std::cos(thetaIR),   cosThetaO);
    float pdfTT  = weightTT *M(_vTT,  std::sin(thetaITT),  sinThetaO, std::cos(thetaITT),  cosThetaO);
    float pdfTRT = weightTRT*M(_vTRT, std::sin(thetaITRT), sinThetaO, std::cos(thetaITRT), cosThetaO);

    return (1.0f/weightSum)*
          (pdfR  *  _nR->pdf(phi, cosThetaD)
         + pdfTT * _nTT->pdf(phi, cosThetaD)
         + pdfTRT*_nTRT->pdf(phi, cosThetaD));
}


void HairBcsdf::precomputeAzimuthalDistributions()
{
    const int Resolution = PrecomputedAzimuthalLobe::AzimuthalResolution;
    std::unique_ptr<Vec3f[]> valuesR  (new Vec3f[Resolution*Resolution]);
    std::unique_ptr<Vec3f[]> valuesTT (new Vec3f[Resolution*Resolution]);
    std::unique_ptr<Vec3f[]> valuesTRT(new Vec3f[Resolution*Resolution]);

    // Ideally we could simply make this a constexpr, but MSVC does not support that yet (boo!)
    #define NumPoints 140

    GaussLegendre<NumPoints> integrator;
    const auto points = integrator.points();
    const auto weights = integrator.weights();

    // Cache the gammaI across all integration points
    std::array<float, NumPoints> gammaIs;
    for (int i = 0; i < NumPoints; ++i)
        gammaIs[i] = std::asin(points[i]);

    // Precompute the Gaussian detector and sample it into three 1D tables.
    // This is the only part of the precomputation that is actually approximate.
    // 2048 samples are enough to support the lowest roughness that the BCSDF
    // can reliably simulate
    const int NumGaussianSamples = 2048;
    std::unique_ptr<float[]> Ds[3];
    for (int p = 0; p < 3; ++p) {
        Ds[p].reset(new float[NumGaussianSamples]);
        for (int i = 0; i < NumGaussianSamples; ++i)
            Ds[p][i] = D(_betaR, i/(NumGaussianSamples - 1.0f)*TWO_PI);
    }

    // Simple wrapped linear interpolation of the precomputed table
    auto approxD = [&](int p, float phi) {
        float u = std::abs(phi*(INV_TWO_PI*(NumGaussianSamples - 1)));
        int x0 = int(u);
        int x1 = x0 + 1;
        u -= x0;
        return Ds[p][x0 % NumGaussianSamples]*(1.0f - u) + Ds[p][x1 % NumGaussianSamples]*u;
    };

    // Here follows the actual precomputation of the azimuthal scattering functions
    // The scattering functions are parametrized with the azimuthal angle phi,
    // and the cosine of the half angle, cos(thetaD).
    // This parametrization makes the azimuthal function relatively smooth and allows using
    // really low resolutions for the table (64x64 in this case) without any visual
    // deviation from ground truth, even at the lowest supported roughness setting
    for (int y = 0; y < Resolution; ++y) {
        float cosHalfAngle = y/(Resolution - 1.0f);

        // Precompute reflection Fresnel factor and reduced absorption coefficient
        float iorPrime = std::sqrt(Eta*Eta - (1.0f - cosHalfAngle*cosHalfAngle))/cosHalfAngle;
        float cosThetaT = std::sqrt(1.0f - (1.0f - cosHalfAngle*cosHalfAngle)*sqr(1.0f/Eta));
        Vec3f sigmaAPrime = _sigmaA/cosThetaT;

        // Precompute gammaT, f_t and internal absorption across all integration points
        std::array<float, NumPoints> fresnelTerms, gammaTs;
        std::array<Vec3f, NumPoints> absorptions;
        for (int i = 0; i < NumPoints; ++i) {
            gammaTs[i] = std::asin(clamp(points[i]/iorPrime, -1.0f, 1.0f));
            fresnelTerms[i] = Fresnel::dielectricReflectance(1.0f/Eta, cosHalfAngle*std::cos(gammaIs[i]));
            absorptions[i] = std::exp(-sigmaAPrime*2.0f*std::cos(gammaTs[i]));
        }

        for (int phiI = 0; phiI < Resolution; ++phiI) {
            float phi = TWO_PI*phiI/(Resolution - 1.0f);

            float integralR = 0.0f;
            Vec3f integralTT(0.0f);
            Vec3f integralTRT(0.0f);

            // Here follows the integration across the fiber width, h.
            // Since we were able to precompute most of the factors that
            // are constant w.r.t phi for a given h,
            // we don't have to do much work here.
            for (int i = 0; i < integrator.numSamples(); ++i) {
                float fR = fresnelTerms[i];
                Vec3f T = absorptions[i];

                float AR = fR;
                Vec3f ATT = (1.0f - fR)*(1.0f - fR)*T;
                Vec3f ATRT = ATT*fR*T;

                integralR   += weights[i]*approxD(0, phi - Phi(gammaIs[i], gammaTs[i], 0))*AR;
                integralTT  += weights[i]*approxD(1, phi - Phi(gammaIs[i], gammaTs[i], 1))*ATT;
                integralTRT += weights[i]*approxD(2, phi - Phi(gammaIs[i], gammaTs[i], 2))*ATRT;
            }

            valuesR  [phiI + y*Resolution] = Vec3f(0.5f*integralR);
            valuesTT [phiI + y*Resolution] = 0.5f*integralTT;
            valuesTRT[phiI + y*Resolution] = 0.5f*integralTRT;
        }
    }

    // Hand the values off to the helper class to construct sampling CDFs and so forth
    _nR  .reset(new PrecomputedAzimuthalLobe(std::move(valuesR)));
    _nTT .reset(new PrecomputedAzimuthalLobe(std::move(valuesTT)));
    _nTRT.reset(new PrecomputedAzimuthalLobe(std::move(valuesTRT)));
}

void HairBcsdf::prepareForRender()
{
    // Roughening/tightening of the different lobes as described in Marschner et al.'s paper.
    // Multiplied with Pi/2 to have similar range as the rough dielectric microfacet.
    // Clamped to some minimum roughness value to avoid oscillations in the azimuthal
    // scattering function.
    _betaR   = max(PI_HALF*_roughness, 0.04f);
    _betaTT  = _betaR*0.5f;
    _betaTRT = _betaR*2.0f;

    _vR   = _betaR  *_betaR;
    _vTT  = _betaTT *_betaTT;
    _vTRT = _betaTRT*_betaTRT;

    _scaleAngleRad = Angle::degToRad(_scaleAngleDeg);

    if (!_overridesSigmaA) {
        // The two melanin parameters allow easy reproduction of physical hair colors
        // based on the mixture of two pigments, eumelanin and pheomelanin, found in human hair
        // These RGB absorption values are taken from "An Energy-Conserving Hair Reflectance Model"
        const Vec3f eumelaninSigmaA = Vec3f(0.419f, 0.697f, 1.37f);
        const Vec3f pheomelaninSigmaA = Vec3f(0.187f, 0.4f, 1.05f);

        _sigmaA = _melaninConcentration*lerp(eumelaninSigmaA, pheomelaninSigmaA, _melaninRatio);
    }

    precomputeAzimuthalDistributions();
}


}
