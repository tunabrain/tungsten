#include "sampling/SampleGenerator.hpp"
#include "math/TangentFrame.hpp"
#include "math/Angle.hpp"
#include "math/Vec.hpp"
#include "IntTypes.hpp"

#include <stdio.h>
#include <vector>

using namespace Tungsten;

int generateHairBall(std::vector<Vec3f> &points, std::vector<float> &thicknesses, UniformSampler &rng)
{
    const int Segments = 64;

    float angleStep = PI/Segments;
    float r = 1.0f + rng.next1D()*0.1f;
    float basePhi = rng.next1D()*TWO_PI;
    float thickness = 0.0005f + rng.next1D()*0.0001f;
    for (int i = -1; i <= Segments + 1; ++i) {
        float phi = basePhi + rng.next1D()*TWO_PI*0.025f/Segments;
        float theta = clamp(i*angleStep, -1e-2f, PI + 1e2f) + rng.next1D()*angleStep*0.02f;
        thicknesses.push_back(thickness);
        points.emplace_back(
            std::sin(theta)*std::cos(phi)*r,
            std::cos(theta)*r,
            std::sin(theta)*std::sin(phi)*r
        );
    }

    return Segments + 2;
}

int generateTuft(std::vector<Vec3f> &points, std::vector<float> &thickness, UniformSampler &rng)
{
    Vec2f size(3.0f, 3.0f);

    Vec2f uv = size*(rng.next2D() - 0.5f);
    Vec3f dir = Vec3f(0.0f, 1.0f, 1.0f).normalized();
    int length = 4 + int(rng.next1D()*8);
    float widthStart = 0.0005f + rng.next1D()*0.0002f;
    float widthStep = widthStart/(length - 1);

    float width = widthStart;
    Vec3f p(uv.x(), 0.0f, uv.y());
    for (int i = 0; i < length; ++i) {
        points.push_back(p);
        thickness.push_back(width);

        //float angle = PI*1.5f + (rng.next1D() - 0.5f)*PI*0.5f;
        float angle = rng.next1D()*TWO_PI;
        float stepSize = (0.4f + rng.next1D()*0.4f)*0.1f;
        Vec3f newDir = Vec3f(std::cos(angle)*0.2f, std::sin(angle)*0.2f, stepSize*10.0f).normalized();
        TangentFrame frame(dir);
        dir += frame.toGlobal(newDir) + Vec3f(0.0f, -0.1f, 0.0f);
        dir.normalize();
        p += dir*stepSize;

        width -= widthStep;
    }

    return length - 1;
}

int generateHair(std::vector<Vec3f> &points, std::vector<float> &thickness, UniformSampler &rng)
{
    Vec2f size(3.0f, 3.0f);

    Vec2f uv = size*(rng.next2D() - 0.5f);
    float angle = PI*1.5f + (rng.next1D() - 0.5f)*PI*0.5f;
    TangentFrame frame(Vec3f(0.0f, 1.0f, 1.0f).normalized());
    Vec3f dir = Vec3f(std::cos(angle), std::sin(angle), 0.0f)*(0.4f + rng.next1D()*0.4f)*0.5f;
    dir = frame.toGlobal(dir);
    float height1 = lerp(0.3f, 0.6f, rng.next1D());
    float height2 = lerp(0.8f, 1.1f, rng.next1D());

    Vec3f p1(uv.x(), 0.0f, uv.y());
    Vec3f p2 = p1 + dir*(0.3f + rng.next1D()*0.3f) + height1*frame.normal;
    Vec3f p3 = p1 + dir*1.0f + height2*frame.normal;
    Vec3f p0 = p1 + (p1 - p2)*0.5f;
    Vec3f p4 = p3 + (p3 - p2)*0.5f;

    points.push_back(p0);
    points.push_back(p1);
    points.push_back(p2);
    points.push_back(p3);
    points.push_back(p4);

    float width0 = 0.005f + rng.next1D()*0.002f;
    float width1 = 0.001f;
    thickness.push_back(width0);
    thickness.push_back(width0);
    thickness.push_back((width0 + width1)*0.5f);
    thickness.push_back(width1);
    thickness.push_back(-width1);

    return 4;
}

int main() {
    UniformSampler rng(0xBA5EBA11);

    std::vector<uint16> segments;
    std::vector<Vec3f> points;
    std::vector<float> thickness;

    for (int i = 0; i < 30000; ++i)
        segments.push_back(generateHairBall(points, thickness, rng));
    std::cout << points.size() << std::endl;

    int curveCount = segments.size();
    int nodeCount = points.size();
    int descriptor = 0;
    descriptor |= 0x01; //hasSegments
    descriptor |= 0x02; //hasPoints
    descriptor |= 0x04; //hasThickness
    uint32 defaultSegments = 5;
    float defaultThickness = 0.01f;
    float defaultTransparency = 1.0f;
    float defaultColor[] = {1.0f, 1.0f, 1.0f};
    char fileInfo[88] = {0};

    FILE *f = fopen("hair-ball.hair", "wb");
    fprintf(f, "HAIR");
    fwrite(&curveCount,          4, 1, f);
    fwrite(&nodeCount,           4, 1, f);
    fwrite(&descriptor,          4, 1, f);
    fwrite(&defaultSegments,     4, 1, f);
    fwrite(&defaultThickness,    4, 1, f);
    fwrite(&defaultTransparency, 4, 1, f);
    fwrite(&defaultColor,        4, 3, f);
    fwrite(fileInfo, 1, 88, f);

    fwrite(&segments[0], 2, segments.size(), f);
    fwrite(&points[0], 4, points.size()*3, f);
    fwrite(&thickness[0], 4, thickness.size(), f);

    fclose(f);

    return 0;
}
