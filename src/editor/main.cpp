#include <embree/include/embree.h>
#include <QDesktopWidget>
#include <QApplication>
#include <QGLWidget>

#include "RenderWindow.hpp"
#include "MainWindow.hpp"

using namespace Tungsten;

#include "bsdfs/RoughDielectricBsdf.hpp"
#include "bsdfs/RoughConductorBsdf.hpp"
#include "bsdfs/SmoothCoatBsdf.hpp"
#include "bsdfs/Microfacet.hpp"
#include "bsdfs/LambertBsdf.hpp"
void microfacetTest()
{
    std::ofstream Dout("D.txt");
    std::ofstream Gout("G.txt");
    std::ofstream Pout("P.txt");

    const int Samples = 360;
    float alpha = 0.2f;
    float alphaP = Microfacet::roughnessToAlpha(Microfacet::Phong, alpha);
    Dout << "# " << alpha << " " << alphaP << " " << alpha << std::endl;
    Gout << "# " << alpha << " " << alphaP << " " << alpha << std::endl;
    Pout << "# " << alpha << " " << alphaP << " " << alpha << std::endl;
    Vec3f wi(std::sin(0.025f*PI), 0.0f, std::cos(0.025f*PI));
    Vec3f wo(std::sin(0.1f*PI), 0.0f, std::cos(0.1f*PI));
    for (int i = 0; i < 360; ++i) {
        float a = (i*PI)/Samples - PI_HALF;
        Vec3f m(std::sin(a), 0.0f, std::cos(a));
        float b = Microfacet::D(Microfacet::Beckmann, alpha, m);
        float p = Microfacet::D(Microfacet::Phong,   alphaP, m);
        float g = Microfacet::D(Microfacet::GGX,      alpha, m);
        Dout << Angle::radToDeg(a) << " " << b << " " << p << " " << g << std::endl;
        b = Microfacet::pdf(Microfacet::Beckmann, alpha, m);
        p = Microfacet::pdf(Microfacet::Phong,   alphaP, m);
        g = Microfacet::pdf(Microfacet::GGX,      alpha, m);
        Pout << Angle::radToDeg(a) << " " << b << " " << p << " " << g << std::endl;
        b = Microfacet::G(Microfacet::Beckmann, alpha, wi, m, wo);
        p = Microfacet::G(Microfacet::Phong,   alphaP, wi, m, wo);
        g = Microfacet::G(Microfacet::GGX,      alpha, wi, m, wo);
        Gout << Angle::radToDeg(a) << " " << b << " " << p << " " << g << std::endl;
    }
    Dout.close();
    Gout.close();
    Pout.close();
}

void bsdfTest()
{
    const float angle = 220.0f*(PI/180.0f);
    std::ofstream Pout("P-Dielectric.txt");
    Vec3f wi(std::sin(angle), 0.0f, std::cos(angle));
    const int Samples = 360;
    RoughDielectricBsdf bsdf;
    SobolSampler sampler1;
    UniformSampler sampler2(123456);
    IntersectionInfo info;
    SurfaceScatterEvent event(&info, &sampler1, &sampler2, wi, BsdfLobes::AllLobes);

    for (int i = 0; i < 360; ++i) {
        float a = (i*TWO_PI)/Samples - PI;
        Vec3f wo(std::sin(a), 0.0f, std::cos(a));
        event.wo = wo;
        Pout << Angle::radToDeg(a) << " " << bsdf.pdf(event) << std::endl;
    }

    Pout.close();
}

void samplingTest()
{
    //RoughDielectricBsdf bsdf;
    SmoothCoatBsdf bsdf;
    UniformSampler sampler1(0xBA5EBA11);
    UniformSampler sampler2(123456);

    int successes = 0;
    double pAvg = 0.0f, pMax = 0.0f;
    double fAvg = 0.0f, fMax = 0.0f;
    double wAvg = 0.0f, wMax = 0.0f;
    const int Samples = 10000000;
    for (int i = 0; i < Samples; ++i) {
        //const float angle = sampler1.next1D()*TWO_PI;//1.0f*(PI/180.0f);
        const float angle = Angle::degToRad(45.0f);
        Vec3f wi(std::sin(angle), 0.0f, std::cos(angle));
        IntersectionInfo info;
        SurfaceScatterEvent event(&info, &sampler1, &sampler2, wi, BsdfLobes::AllButSpecular);

        if (!bsdf.sample(event))
            continue;

        float pdf = bsdf.pdf(event);
        float f = bsdf.eval(event).x();
        float fe = std::abs(event.throughput.x()*event.pdf - f)/f;
        float pe = std::abs(event.pdf - pdf)/pdf;
        if (std::isnan(event.throughput.x()) || std::isnan(event.pdf))
            std::cout << "Event failure: " << event.throughput.x() << " " << event.pdf << std::endl;
        else if (std::isnan(pdf) || std::isnan(f))
            std::cout << "Baseline failure: " << pdf << " " << f << std::endl;
        else {
            successes++;
            fAvg += fe;
            wAvg += event.throughput.x();
            pAvg += pe;
            fMax = max(double(fe), fMax);
            wMax = max(double(event.throughput.x()), wMax);
            if (pe > pMax) {
                std::cout << pe << " " << event.pdf << " " << pdf << " " << event.wi << " " << event.wo << std::endl;
            }
            pMax = max(double(pe), pMax);
        }
    }

    std::cout.precision(10);
    std::cout << "Pdf:     Avg " << pAvg/successes << " Max " << pMax << std::endl;
    std::cout << "Bsdf:    Avg " << fAvg/successes << " Max " << fMax << std::endl;
    std::cout << "Weights: Avg " << wAvg/successes << " Max " << wMax << std::endl;
    std::cout << "Successes: " << successes << std::endl;
}

#include "mitsuba/Diffuse.hpp"
#include "mitsuba/RoughDielectric.hpp"
#include "mitsuba/RoughConductor.hpp"
#include "mitsuba/Coating.hpp"
void mitsubaVerify()
{
    SmoothCoatBsdf *bsdfTungsten = new SmoothCoatBsdf();
    //RoughConductorBsdf *bsdfTungsten = new RoughConductorBsdf();
    Mitsuba::RoughConductor *roughMetal = new Mitsuba::RoughConductor(Vec3d(0.214000f, 0.950375f, 1.177500f), Vec3d(3.670000f, 2.576500f, 2.160063f), 0.1f);
    Mitsuba::SmoothCoating *bsdfMitsuba = new Mitsuba::SmoothCoating(1.3f, 1.0f, roughMetal);
    //Mitsuba::SmoothCoating *bsdfMitsuba = new Mitsuba::SmoothCoating(1.3f, 1.0f, new Mitsuba::SmoothDiffuse(Mitsuba::Spectrum(1.0f)));

    const float angle = 45.0f*(PI/180.0f);
    std::ofstream Fout("Verify-F.txt");
    std::ofstream Pout("Verify-P.txt");
    Vec3f wi(std::sin(angle), 0.0f, std::cos(angle));

    UniformSampler sampler1(0xBA5EBA11);
    UniformSampler sampler2(123456);

    const int Samples = 360;
    for (int i = 0; i < Samples; ++i) {
        float a = (i*TWO_PI)/Samples - PI;
        Vec3f wo(std::sin(a), 0.0f, std::cos(a));

        Mitsuba::BSDFSamplingRecord bRec;
        bRec.wi = Vec3d(wi);
        bRec.wo = Vec3d(wo);
        IntersectionInfo info;
        SurfaceScatterEvent event(&info, &sampler1, &sampler2, wi, BsdfLobes::AllButSpecular);
        event.wo = wo;

        Pout << Angle::radToDeg(a) << " " << bsdfTungsten->pdf(event) << " " << bsdfMitsuba->pdf(bRec) << std::endl;
        Fout << Angle::radToDeg(a) << " " << bsdfTungsten->eval(event).x() << " " << bsdfMitsuba->eval(bRec).x() << std::endl;
    }

    Pout.close();
    Fout.close();
}

Vec3f sampleRayleigh(SampleGenerator *sampler)
{
    Vec2f sample(sampler->next2D());

    float z = 2 * (2*sample.x() - 1),
          tmp = std::sqrt(z*z+1),
          A = std::pow(z+tmp, (float) (1.0f/3.0f)),
          B = std::pow(z-tmp, (float) (1.0f/3.0f)),
          cosTheta = A + B,
          sinTheta = std::sqrt(max(1.0f-cosTheta*cosTheta, 0.0f)),
          phi = 2*M_PI*sample.y(),
          cosPhi = std::cos(phi),
          sinPhi = std::sin(phi);

    return Vec3f(
        sinTheta * cosPhi,
        sinTheta * sinPhi,
        cosTheta);
}

void phaseFunctionTest()
{
    UniformSampler sampler(0xBA5EBA11);

    for (int i = 0; i < 50; ++i)
        std::cout << sampleRayleigh(&sampler) << std::endl;
}

const float Rg = 6360.0f;
float analyticTransmittance(float h, float r, float mu, float d)
{
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

float numericTransmittance(float h, float r, float mu, float d)
{
    const int numSteps = 1024*100;
    float dD = d/numSteps;
    float t = 0.0f;
    float result = 0.0f;
    for (int i = 0; i <= numSteps; ++i) {
        float rPrime = std::sqrt(r*r + t*t + 2.0f*r*t*mu);
        result += std::exp((Rg - rPrime)/h)*dD;
        t += dD;
    }
    return result;
}

Vec2f densityAndDerivative(float h, float r, float mu, float t)
{
    float d = std::sqrt(r*r + t*t + 2.0f*r*t*mu);
    float density = std::exp((Rg - d)/h);
    float dDdT = std::abs((t + r*mu)/(d*h))*max(density, 0.01f);//*density;
    return Vec2f(density, dDdT);
}

float cleverTransmittance(float h, float r, float mu, float d)
{
    constexpr float maxError = 0.02f;

    int numSteps = 0;

    float t = 0.0f;
    float opticalDepth = 0.0f;
    while (t < d) {
        Vec2f dD = densityAndDerivative(h, r, mu, t);
        //std::cout << dD.y() << std::endl;
        float dT = maxError/dD.y();
        opticalDepth += dD.x()*min(d - t, dT);
        t += dT;

        numSteps++;
    }

    //std::cout << numSteps << " steps" << std::endl;

    return opticalDepth;
}

float cleverTransmittance2(float h, float r, float mu, float d, int &steps)
{
    constexpr float maxError = 0.02f;

    int numSteps = 0;

    float opticalDepth = 0.0f;
    Vec2f dD = densityAndDerivative(h, r, mu, 0.0f);
    float dT = min(maxError/dD.y(), d*0.2f);
    float t = dT;
    while (t < d) {
        Vec2f newDd = densityAndDerivative(h, r, mu, t);
        opticalDepth += (dD.x() + newDd.x())*0.5f*dT;

        dD = newDd;
        dT = min(maxError/dD.y(), d*0.2f);
        t += dT;
        numSteps++;
    }
    Vec2f newDd = densityAndDerivative(h, r, mu, d);
    opticalDepth += (dD.x() + newDd.x())*0.5f*(d - t + dT);

    //std::cout << numSteps << " steps" << std::endl;

    steps = numSteps + 2;

    return opticalDepth;
}

void transmittanceTest()
{
    int steps;
    std::cout << analyticTransmittance(8.0f, Rg + 5.0f, 1.0f, 1000.0f)*2 << std::endl;
    std::cout <<  numericTransmittance(8.0f, Rg + 5.0f, 1.0f, 1000.0f) << std::endl;
    std::cout <<  cleverTransmittance2(8.0f, Rg + 5.0f, 1.0f, 1000.0f, steps) << std::endl;
    std::cout <<   cleverTransmittance(8.0f, Rg + 5.0f, 1.0f, 1000.0f) << std::endl;
//  std::cout << analyticTransmittance(800.0f, Rg + 6001.0f, -1.0f, 6000.0f)*2 << std::endl;
//  std::cout <<  numericTransmittance(800.0f, Rg + 6001.0f, -1.0f, 6000.0f) << std::endl;
//  std::cout <<  cleverTransmittance2(800.0f, Rg + 6001.0f, -1.0f, 6000.0f, steps) << std::endl;
//  std::cout <<   cleverTransmittance(800.0f, Rg + 6001.0f, -1.0f, 6000.0f) << std::endl;

    std::cout << "#steps: " << steps << std::endl;
}

void loadPfm(const char *filename)
{
    std::ifstream in(filename, std::ios_base::in | std::ios_base::binary);
    if (!in.good())
        return;

    std::string ident;
    in >> ident;
    int channels;
    if (ident == "Pf")
        channels = 1;
    else if (ident == "PF")
        channels = 3;
    else
        return;

    int w, h;
    in >> w >> h;
    double s;
    in >> s;
    std::string tmp;
    std::getline(in, tmp);

    float *img = new float[w*h*channels];
    in.read(reinterpret_cast<char *>(img), w*h*channels*sizeof(float));
    in.close();

    uint8 *ldr = new uint8[w*h*3];
    for (int i = 0; i < w*h; ++i) {
        int idx = i*channels;
        float r, g, b;
        if (channels == 1) {
            r = g = b = img[idx];
        } else {
            r = img[idx + 0];
            g = img[idx + 1];
            b = img[idx + 2];
        }
        ldr[i*3 + 0] = uint8(min(int(255.0f*r), 255));
        ldr[i*3 + 1] = uint8(min(int(255.0f*g), 255));
        ldr[i*3 + 2] = uint8(min(int(255.0f*b), 255));
    }

    lodepng_encode24_file("Test.png", ldr, w, h);
}

int main(int argc, char **argv)
{
    //phaseFunctionTest();
    //transmittanceTest();
    //return 0;

    QApplication app(argc, argv);

    QDesktopWidget desktop;
    QRect windowSize(desktop.screenGeometry(desktop.primaryScreen()));
    windowSize.adjust(100, 100, -100, -100);

    MainWindow mainWindow;
    mainWindow.setWindowTitle("Tungsten Scene Editor");
    mainWindow.setGeometry(windowSize);

    mainWindow.show();

    embree::rtcInit();
    //embree::rtcSetVerbose(1);
    embree::rtcStartThreads(8);

    try {
        return app.exec();
    } catch (std::runtime_error &e) {
        std::cout << e.what() << std::endl;
        return 1;
    }
}
