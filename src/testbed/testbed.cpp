#include "math/Vec.hpp"
#include "math/MathUtil.hpp"
#include "extern/tinyformat/tinyformat.hpp"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>

using namespace Tungsten;

std::vector<Vec2f> points;
GLFWwindow *window;

// http://www.answers.com/topic/b-spline
template<typename T>
T quadraticBSpline(T p0, T p1, T p2, float t)
{
    return (1.0f/2.0f)*(
        (      p0 - 2.0f*p1 + p2)*t*t +
        (-2.0f*p0 + 2.0f*p1     )*t +
        (      p0 +      p1     )
    );
}

template<typename T>
T quadraticBSplineDeriv(T p0, T p1, T p2, float t)
{
    return (p0 - 2.0f*p1 + p2)*t + (p1 - p0);
}

Vec2f minMaxQuadratic(float p0, float p1, float p2)
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

bool pointOnSpline(Vec2f p0, Vec2f p1, Vec2f p2, const float w0, const float w1, const float w2, const Vec2f q, Vec2f &uv)
{
    constexpr int MaxDepth = 10;

    struct StackNode
    {
        float tMin, tSpan;
        Vec2f p0, p1;
        int depth;

        void set(float tMin_, float tSpan_, Vec2f p0_, Vec2f p1_, int depth_)
        {
            tMin = tMin_;
            tSpan = tSpan_;
            p0 = p0_;
            p1 = p1_;
            depth = depth_;
        }
    };

    p0 -= q;
    p1 -= q;
    p2 -= q;

    Vec2f tFlat = (p0 - p1)/(p0 - 2.0f*p1 + p2);
    float xFlat = quadraticBSpline(p0.x(), p1.x(), p2.x(), tFlat.x());
    float yFlat = quadraticBSpline(p0.y(), p1.y(), p2.y(), tFlat.y());

    StackNode stackBuf[MaxDepth];
    const StackNode *top = &stackBuf[0];
    StackNode *stack = &stackBuf[0];

    StackNode cur{0.0f, 1.0f, (p0 + p1)*0.5f, (p1 + p2)*0.5f, 0};

    bool foundPoint = false;
    float closestDistance = max(w0, w1, w2);

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

        float testWidth = max(quadraticBSpline(w0, w1, w2, cur.tMin), quadraticBSpline(w0, w1, w2, cur.tMin + cur.tSpan));

        if (pMin.x() <= testWidth && pMin.y() <= testWidth && pMax.x() >= -testWidth && pMax.y() >= -testWidth) {
            if (cur.depth >= MaxDepth) {
                Vec2f v = (cur.p1 - cur.p0);
                float lengthSq = v.lengthSq();
                float segmentT = -(cur.p0.dot(v))/lengthSq;
                float distance;
                if (segmentT <= 0.0f)
                    distance = cur.p0.length();
                else if (segmentT >= 1.0f)
                    distance = cur.p1.length();
                else
                    distance = std::fabs(cur.p0.x()*v.y() - cur.p0.y()*v.x())/std::sqrt(lengthSq);

                float newT = segmentT*cur.tSpan + cur.tMin;
                float currentWidth = quadraticBSpline(w0, w1, w2, newT);
                if (distance < closestDistance && distance < currentWidth && newT >= 0.0f && newT <= 1.0f) {
                    uv = Vec2f(newT, distance/currentWidth);
                    closestDistance = distance;
                    foundPoint = true;
                }
            } else {
                float newSpan = cur.tSpan*0.5f;
                float splitT = cur.tMin + newSpan;
                Vec2f pSplit = quadraticBSpline(p0, p1, p2, splitT);
                (*stack++).set(cur.tMin, newSpan, cur.p0, pSplit, cur.depth + 1);
                cur.set(splitT, newSpan, pSplit, cur.p1, cur.depth + 1);
                continue;
            }
        }
        if (stack == top)
            break;
        cur = *--stack;
    }

    return foundPoint;
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
    if (glfwGetKey(window, GLFW_KEY_BACKSPACE))
        points.clear();

    double x, y;
    glfwGetCursorPos(window, &x, &y);

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
        const float width = 10.0f;

        for (int i = 0; i < count; ++i) {
            float w0 = (width*max(i - 1, 0))/count;
            float w1 = (width*i)/count;
            float w2 = (width*min(i + 1, count - 1))/count;

            Vec2f p0 = points[max(i - 1, 0)];
            Vec2f p1 = points[max(i, 0)];
            Vec2f p2 = points[min(i + 1, count - 1)];

            Vec2f minMaxX = minMaxQuadratic(p0.x(), p1.x(), p2.x());
            Vec2f minMaxY = minMaxQuadratic(p0.y(), p1.y(), p2.y());

            glBegin(GL_LINE_LOOP);
            glColor3f(1.0, 1.0, 1.0);
            glVertex2f(minMaxX.x(), minMaxY.x());
            glVertex2f(minMaxX.x(), minMaxY.y());
            glVertex2f(minMaxX.y(), minMaxY.y());
            glVertex2f(minMaxX.y(), minMaxY.x());
            glEnd();

            glBegin(GL_TRIANGLE_STRIP);
            Vec2f uv;
            if (pointOnSpline(p0, p1, p2, w0, w1, w2, Vec2f(float(x), float(y)), uv))
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
