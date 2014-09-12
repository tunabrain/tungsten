#ifndef RENDER_SHADER_HPP_
#define RENDER_SHADER_HPP_

#include <GL/glew.h>
#include "ShaderObject.hpp"

#include "math/Vec.hpp"
#include "math/Mat4f.hpp"

namespace Tungsten {

namespace GL {

#define MAX_SHADERS 8
#define MAX_OUTPUTS 8
#define MAX_VARYINGS 8
#define MAX_UNIFORMS 32

enum FeedbackMode
{
    FEEDBACK_INTERLEAVED,
    FEEDBACK_SEPARATE
};

union UniformValue
{
    int i;
    float f;
    int i2[2];
    int i3[3];
    int i4[3];
    float f2[2];
    float f3[3];
    float f4[4];
};

class Shader
{
    GLuint _program;

    int _shaderCount;
    struct ShaderObject _shaders[MAX_SHADERS];
    int _outputCount;
    const char *_outputs[MAX_OUTPUTS];
    int _varyingCount;
    const char *_varyings[MAX_VARYINGS];
    enum FeedbackMode _feedbackMode;

    int _uniformCount;
    int _uniformHash[MAX_UNIFORMS];
    int _uniformLocation[MAX_UNIFORMS];
    union UniformValue _uniformVals[MAX_UNIFORMS];

    int uniformIndex(const char *name);
    void check();

public:
    Shader()
    : _program(-1), _shaderCount(0), _outputCount(0), _varyingCount(0),
      _feedbackMode(FEEDBACK_INTERLEAVED), _uniformCount(0)
    {}

    Shader(const char *prefix, const char *preamble, const char *v, const char *g,
            const char *f, int outputs);
    Shader(const char *prefix, const char *preamble, const char *c);

    ShaderObject *addObject();
    void addOutput(const char *name);
    void addFeedbackVarying(const char *name);
    void setFeedbackMode(FeedbackMode f);

    int refresh();
    void link();
    void bind();

    void dispatch(int sizeX, int sizeY = 1, int sizeZ = 1);

    GLint uniform(const char *name);
    void uniformI(const char *name, int f1);
    void uniformI(const char *name, int f1, int f2);
    void uniformI(const char *name, int f1, int f2, int f3);
    void uniformI(const char *name, int f1, int f2, int f3, int f4);
    void uniformF(const char *name, float f1);
    void uniformF(const char *name, float f1, float f2);
    void uniformF(const char *name, float f1, float f2, float f3);
    void uniformF(const char *name, float f1, float f2, float f3, float f4);
    void uniformF(const char *name, const Vec3f &v);
    void uniformF(const char *name, const Vec4f &v);
    void uniformMat(const char *name, const Mat4f &v, bool transpose);

    GLint program() const
    {
        return _program;
    }
};

}

}

#endif /* RENDER_SHADER_HPP_ */
