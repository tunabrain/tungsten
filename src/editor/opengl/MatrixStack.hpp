#ifndef RENDER_MATRIXSTACK_HPP_
#define RENDER_MATRIXSTACK_HPP_

#include "math/Mat4f.hpp"

#include <stack>

namespace Tungsten {

namespace GL {

enum StackName {
    PROJECTION_STACK,
    MODEL_STACK,
    VIEW_STACK,
    /* Virtual stacks */
    MODELVIEW_STACK,
    MODELVIEWPROJECTION_STACK,
    INV_MODEL_STACK,
    INV_VIEW_STACK,
    INV_MODELVIEW_STACK,

    MATRIX_STACK_COUNT
};

enum StackFlag {
    PROJECTION_FLAG          = (1 << 0),
    MODEL_FLAG               = (1 << 1),
    VIEW_FLAG                = (1 << 2),
    /* Virtual stacks */
    MODELVIEW_FLAG           = (1 << 3),
    MODELVIEWPROJECTION_FLAG = (1 << 4),
    INV_MODEL_FLAG           = (1 << 5),
    INV_VIEW_FLAG            = (1 << 6),
    INV_MODELVIEW_FLAG       = (1 << 7)
};

class Shader;

class MatrixStack {
    static std::stack<Mat4f> _stacks[];

    MatrixStack();

public:
    static void set(StackName n, const Mat4f &m);
    static void mulR(StackName n, const Mat4f &m);
    static void mulL(StackName n, const Mat4f &m);
    static void get(StackName n, Mat4f &m);

    static void copyPush(StackName n);
    static void push(StackName n);
    static void pop(StackName n);

    static void setShaderMatrices(Shader &s, int flags);
};

}

}

#endif /* RENDER_MATRIXSTACK_HPP_ */
