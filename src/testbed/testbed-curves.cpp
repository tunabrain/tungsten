#include "math/Vec.hpp"
#include "math/MathUtil.hpp"
#include "extern/tinyformat/tinyformat.hpp"
#include "extern/lodepng/lodepng.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>

using namespace Tungsten;

std::vector<Vec2f> points;
GLFWwindow *window;

// http://www.answers.com/topic/b-spline
template<typename T>
inline T quadraticBSpline(T p0, T p1, T p2, float t)
{
    return (0.5f*p0 - p1 + 0.5f*p2)*t*t + (p1 - p0)*t + 0.5f*(p0 + p1);
}

template<typename T>
inline T quadraticBSplineDeriv(T p0, T p1, T p2, float t)
{
    return (p0 - 2.0f*p1 + p2)*t + (p1 - p0);
}

inline Vec2f minMaxQuadratic(float p0, float p1, float p2)
{
    float xMin = (p0 + p1)*0.5f;
    float xMax = (p1 + p2)*0.5f;
    if (xMin > xMax)
        std::swap(xMin, xMax);

    float tFlat = (p0 - p1)/(p0 - 2.0f*p1 + p2);
    if (tFlat > 0.0f && tFlat < 1.0f) {
        float xFlat = quadraticBSpline(p0, p1, p2, tFlat);
        xMin = min(xMin, xFlat);
        xMax = max(xMax, xFlat);
    }
    return Vec2f(xMin, xMax);
}

bool pointOnSpline(const Vec4f &q0, const Vec4f &q1, const Vec4f &q2, float tMin, float tMax, float &t, Vec2f &uv)
{
    constexpr int MaxDepth = 10;

    struct StackNode
    {
        Vec2f p0, p1;
        float w0, w1;
        float tMin, tSpan;
        int depth;

        void set(float tMin_, float tSpan_, Vec2f p0_, Vec2f p1_, float w0_, float w1_, int depth_)
        {
            p0 = p0_;
            p1 = p1_;
            w0 = w0_;
            w1 = w1_;
            tMin = tMin_;
            tSpan = tSpan_;
            depth = depth_;
        }
    };

    Vec2f p0 = q0.xy(), p1 = q1.xy(), p2 = q2.xy();
    Vec2f tFlat = (p0 - p1)/(p0 - 2.0f*p1 + p2);
    float xFlat = quadraticBSpline(p0.x(), p1.x(), p2.x(), tFlat.x());
    float yFlat = quadraticBSpline(p0.y(), p1.y(), p2.y(), tFlat.y());

    Vec2f deriv1(p0 - 2.0f*p1 + p2), deriv2(p1 - p0);

    StackNode stackBuf[MaxDepth];
    const StackNode *top = &stackBuf[0];
    StackNode *stack = &stackBuf[0];

    StackNode cur{(p0 + p1)*0.5f, (p1 + p2)*0.5f, (q0.w() + q1.w())*0.5f, (q1.w() + q2.w())*0.5f, 0.0f, 1.0f, 0};

    float closestDepth = tMax;

    while (true) {
        Vec2f pMin = min(cur.p0, cur.p1);
        Vec2f pMax = max(cur.p0, cur.p1);
        if (tFlat.x() > cur.tMin && tFlat.x() < cur.tMin + cur.tSpan) {
            pMin.x() = min(pMin.x(), xFlat);
            pMax.x() = max(pMax.x(), xFlat);
        }
        if (tFlat.y() > cur.tMin && tFlat.y() < cur.tMin + cur.tSpan) {
            pMin.y() = min(pMin.y(), yFlat);
            pMax.y() = max(pMax.y(), yFlat);
        }

        float testWidth = max(cur.w0, cur.w1);
        if (pMin.x() <= testWidth && pMin.y() <= testWidth && pMax.x() >= -testWidth && pMax.y() >= -testWidth) {
            if (cur.depth >= MaxDepth) {
                Vec2f tangent0 = deriv2 + deriv1*cur.tMin;
                Vec2f tangent1 = deriv2 + deriv1*(cur.tMin + cur.tSpan);

                if (tangent0.dot(cur.p0) <= 0.0f && tangent1.dot(cur.p1) >= 0.0f) {
                    Vec2f v = (cur.p1 - cur.p0);
                    float lengthSq = v.lengthSq();
                    float segmentT = -(cur.p0.dot(v))/lengthSq;
                    float distance;
                    float signedUnnormalized = cur.p0.x()*v.y() - cur.p0.y()*v.x();
                    if (segmentT <= 0.0f)
                        distance = cur.p0.length();
                    else if (segmentT >= 1.0f)
                        distance = cur.p1.length();
                    else
                        distance = std::fabs(signedUnnormalized)/std::sqrt(lengthSq);

                    float newT = segmentT*cur.tSpan + cur.tMin;
                    float currentWidth = quadraticBSpline(q0.w(), q1.w(), q2.w(), newT);
                    float currentDepth = quadraticBSpline(q0.z(), q1.z(), q2.z(), newT);
                    if (currentDepth < tMax && currentDepth > tMin && distance < currentWidth && newT >= 0.0f && newT <= 1.0f) {
                        float halfDistance = 0.5f*distance/currentWidth;
                        uv = Vec2f(newT, signedUnnormalized < 0.0f ? 0.5f - halfDistance : halfDistance + 0.5f);
                        t = currentDepth;
                        closestDepth = currentDepth;
                    }
                }
            } else {
                float newSpan = cur.tSpan*0.5f;
                float splitT = cur.tMin + newSpan;
                Vec4f qSplit = quadraticBSpline(q0, q1, q2, splitT);
                (*stack++).set(cur.tMin, newSpan, cur.p0, qSplit.xy(), cur.w0, qSplit.w(), cur.depth + 1);
                cur.set(splitT, newSpan, qSplit.xy(), cur.p1, qSplit.w(), cur.w1, cur.depth + 1);
                continue;
            }
        }
        if (stack == top)
            break;
        cur = *--stack;
    }

    return closestDepth < tMax;
}

float cubicBSpline(float p0, float p1, float p2, float p3, float t)
{
    return (1.0f/6.0f)*(
        (     -p0 + 3.0f*p1 - 3.0f*p2 + p3)*t*t*t +
        ( 3.0f*p0 - 6.0f*p1 + 3.0f*p2     )*t*t +
        (-3.0f*p0           + 3.0f*p2     )*t +
        (      p0 + 4.0f*p1 +      p2     )
    );
}

void render()
{
    const int NumSteps = 100;
    int count = points.size();

    glClear(GL_COLOR_BUFFER_BIT);

    if (points.size() < 2)
        return;

//  glBegin(GL_LINE_STRIP);
//  glColor3f(1.0, 0.0, 0.0);
//  for (const Vec2f &p : points)
//      glVertex2f(p.x(), p.y());
//  glEnd();
//
//

    double cx, cy;
    glfwGetCursorPos(window, &cx, &cy);
    float x = cx, y = cy;

    if (glfwGetKey(window, GLFW_KEY_ENTER)) {
        static uint8 buf[1280*720*3];

        const float width = 30.0f;

        for (int y = 0; y < 720; ++y) {
            for (int x = 0; x < 1280; ++x) {
                for (int i = 0; i < count; ++i) {
                    float w0 = (width*max(i - 1, 0))/count;
                    float w1 = (width*i)/count;
                    float w2 = (width*min(i + 1, count - 1))/count;

                    float z0 = -0.5f;
                    float z1 = 0.5f;
                    float z2 = 1.5f;

                    Vec2f p0 = points[max(i - 1, 0)];
                    Vec2f p1 = points[max(i, 0)];
                    Vec2f p2 = points[min(i + 1, count - 1)];

                    Vec4f q0(p0.x() - x, p0.y() - y, z0, w0);
                    Vec4f q1(p1.x() - x, p1.y() - y, z1, w1);
                    Vec4f q2(p2.x() - x, p2.y() - y, z2, w2);

                    int idx = x + y*1280;

                    Vec2f uv;
                    float depth;
                    if (pointOnSpline(q0, q1, q2, -1.0f, 5.0f, depth, uv)) {
                        uint8 d = clamp(int(depth*255.0f), 0, 255);
                        uint8 r = clamp(int(uv.x()*255.0f), 0, 255)*0;
                        uint8 g = clamp(int(uv.y()*255.0f), 0, 255)*0;
                        buf[idx*3 + 0] = d;
                        buf[idx*3 + 1] = d;
                        buf[idx*3 + 2] = d;
                    }
                }
            }
        }

        lodepng_encode24_file("Traced.png", buf, 1280, 720);
    }




    if (glfwGetKey(window, GLFW_KEY_BACKSPACE))
        points.clear();

    if (glfwGetKey(window, GLFW_KEY_SPACE)) {
        glBegin(GL_LINE_STRIP);
        glColor3f(1.0, 1.0, 1.0);

        for (int i = -1; i < count; ++i) {
            for (int j = 0; j < NumSteps; ++j) {
                float t = float(j)/NumSteps;

                float x = cubicBSpline(points[max(i - 1, 0)].x(), points[max(i, 0)].x(), points[min(i + 1, count - 1)].x(), points[min(i + 2, count - 1)].x(), t);
                float y = cubicBSpline(points[max(i - 1, 0)].y(), points[max(i, 0)].y(), points[min(i + 1, count - 1)].y(), points[min(i + 2, count - 1)].y(), t);

                glVertex2f(x, y);
            }
        }
        glEnd();
    } else {
        const float width = 40.0f;

        for (int i = 0; i < count; ++i) {
            float w0 = (width*max(i - 1, 0))/count;
            float w1 = (width*i)/count;
            float w2 = (width*min(i + 1, count - 1))/count;

            float z0 = 0.0f;
            float z1 = 1.0f;
            float z2 = 2.0f;

            Vec2f p0 = points[max(i - 1, 0)];
            Vec2f p1 = points[max(i, 0)];
            Vec2f p2 = points[min(i + 1, count - 1)];

            Vec4f q0(p0.x() - x, p0.y() - y, z0, w0);
            Vec4f q1(p1.x() - x, p1.y() - y, z1, w1);
            Vec4f q2(p2.x() - x, p2.y() - y, z2, w2);

//          Vec2f minMaxX = minMaxQuadratic(p0.x(), p1.x(), p2.x());
//          Vec2f minMaxY = minMaxQuadratic(p0.y(), p1.y(), p2.y());
//
//          glBegin(GL_LINE_LOOP);
//          glColor3f(1.0, 1.0, 1.0);
//          glVertex2f(minMaxX.x(), minMaxY.x());
//          glVertex2f(minMaxX.x(), minMaxY.y());
//          glVertex2f(minMaxX.y(), minMaxY.y());
//          glVertex2f(minMaxX.y(), minMaxY.x());
//          glEnd();

            glBegin(GL_TRIANGLE_STRIP);
            Vec2f uv;
            float depth;
            if (pointOnSpline(q0, q1, q2, -1.0f, 5.0f, depth, uv))
                glColor3f(1.0, 1.0, 1.0);
            else
                glColor3f(0.0, 1.0, 0.0);
            for (int j = 0; j <= NumSteps; ++j) {
                float t = float(j)/NumSteps;
                Vec2f d = quadraticBSplineDeriv(p0, p1, p2, t).normalized()*quadraticBSpline(w0, w1, w2, t);
                Vec2f p = quadraticBSpline(p0, p1, p2, t);
                glVertex2f(p.x() - d.y(), p.y() + d.x());
                glVertex2f(p.x() + d.y(), p.y() - d.x());
            }
            glEnd();

            //if (i > 0 && i < count - 1) {
//              glColor3f(1.0, 0.0, 0.0);
//              glBegin(GL_LINES);
//              Vec2f uv;
//              bool result = pointOnSpline(p0, p1, p2, Vec2f(float(x), float(y)), width, uv);
//              glEnd();
//
//              if (result) {
//                  Vec2f p = quadraticBSpline(p0, p1, p2, uv.x());
//                  glPointSize(5.0f);
//                  glBegin(GL_POINTS);
//                  glVertex2f(p.x(), p.y());
//                  glEnd();
//              }
            //}
        }
    }
}

int main(void)
{
    /* Initialize the library */
    if (!glfwInit())
        return -1;

    glfwWindowHint(GLFW_SAMPLES, 16);
    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(1280, 720, "", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }


    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    glMatrixMode(GL_PROJECTION);
    gluOrtho2D(0.0f, 1280.0f, 720.0f, 0.0f);

    glfwSetMouseButtonCallback(window, [](GLFWwindow *window, int button, int action, int /*mods*/) {
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
            double x, y;
            glfwGetCursorPos(window, &x, &y);
            points.emplace_back(float(x), float(y));
        }
    });

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        render();

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
