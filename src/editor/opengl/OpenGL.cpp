#include "OpenGL.hpp"

namespace Tungsten {

namespace GL {

void initOpenGL()
{
#ifndef __APPLE__
    glewExperimental = GL_TRUE;
    glewInit();
    glGetError();
#endif
}

}

}
