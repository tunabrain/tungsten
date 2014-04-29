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
    SurfaceScatterEvent event(IntersectionInfo(), sampler1, sampler2, wi, BsdfLobes::AllLobes);

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
        SurfaceScatterEvent event(IntersectionInfo(), sampler1, sampler2, wi, BsdfLobes::AllButSpecular);

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
        SurfaceScatterEvent event(IntersectionInfo(), sampler1, sampler2, wi, BsdfLobes::AllButSpecular);
        event.wo = wo;

        Pout << Angle::radToDeg(a) << " " << bsdfTungsten->pdf(event) << " " << bsdfMitsuba->pdf(bRec) << std::endl;
        Fout << Angle::radToDeg(a) << " " << bsdfTungsten->eval(event).x() << " " << bsdfMitsuba->eval(bRec).x() << std::endl;
    }

    Pout.close();
    Fout.close();
}

int main(int argc, char **argv)
{
    //samplingTest();
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
    embree::rtcSetVerbose(1);
    embree::rtcStartThreads(8);

    try {
        return app.exec();
    } catch (std::runtime_error &e) {
        std::cout << e.what() << std::endl;
        return 1;
    }
}
