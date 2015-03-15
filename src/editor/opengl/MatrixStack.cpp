#include "MatrixStack.hpp"
#include "Shader.hpp"

#include "Debug.hpp"

#include <deque>

namespace Tungsten {

namespace GL {

std::stack<Mat4f> MatrixStack::_stacks[3] = {
        std::stack<Mat4f>(std::deque<Mat4f>(1, Mat4f())),
        std::stack<Mat4f>(std::deque<Mat4f>(1, Mat4f())),
        std::stack<Mat4f>(std::deque<Mat4f>(1, Mat4f()))
};

static const char *uniformNames[] = {
    "Projection",
    "Model",
    "View",
    "ModelView",
    "ModelViewProjection",
    "InvModel",
    "InvView",
    "InvModelView",
};

void MatrixStack::set(StackName n, const Mat4f &m)
{
    ASSERT(n <= VIEW_STACK, "Cannot manipulate virtual stacks\n");
    _stacks[n].top() = m;
}

void MatrixStack::mulR(StackName n, const Mat4f &m)
{
    ASSERT(n <= VIEW_STACK, "Cannot manipulate virtual stacks\n");
    _stacks[n].top() = _stacks[n].top()*m;
}

void MatrixStack::mulL(StackName n, const Mat4f &m)
{
    ASSERT(n <= VIEW_STACK, "Cannot manipulate virtual stacks\n");
    _stacks[n].top() = m*_stacks[n].top();
}

void MatrixStack::get(StackName n, Mat4f &m)
{
    switch(n) {
    case PROJECTION_STACK:
    case MODEL_STACK:
    case VIEW_STACK:
        m = _stacks[n].top();
        break;
    case MODELVIEW_STACK:
        m = _stacks[VIEW_STACK].top().pseudoInvert()*_stacks[MODEL_STACK].top();
        break;
    case MODELVIEWPROJECTION_STACK:
        m = _stacks[PROJECTION_STACK].top()*
            _stacks[VIEW_STACK].top().pseudoInvert()*
            _stacks[MODEL_STACK].top();
        break;
    case INV_MODEL_STACK:
        m = _stacks[MODEL_STACK].top().pseudoInvert();
        break;
    case INV_VIEW_STACK:
        m = _stacks[VIEW_STACK].top().pseudoInvert();
        break;
    case INV_MODELVIEW_STACK:
        m = _stacks[MODEL_STACK].top().pseudoInvert()*_stacks[VIEW_STACK].top();
        break;
    default:
        FAIL("Invalid matrix stack\n");
    }
}

void MatrixStack::copyPush(StackName n)
{
    ASSERT(n <= VIEW_STACK, "Cannot manipulate virtual stacks\n");
    _stacks[n].push(_stacks[n].top());
}

void MatrixStack::push(StackName n)
{
    ASSERT(n <= VIEW_STACK, "Cannot manipulate virtual stacks\n");
    _stacks[n].push(Mat4f());
}

void MatrixStack::pop(StackName n)
{
    ASSERT(n <= VIEW_STACK, "Cannot manipulate virtual stacks\n");
    _stacks[n].pop();
}

void MatrixStack::setShaderMatrices(Shader &s, int flags)
{
    Mat4f m;
    for (int i = 0; i < MATRIX_STACK_COUNT; i++) {
        if (flags & (1 << i)) {
            get(StackName(i), m);

            s.uniformMat(uniformNames[i], m, true);
        }
    }
}

}

}
