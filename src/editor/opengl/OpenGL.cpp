#include <QGLWidget>
#include "OpenGL.hpp"

namespace Tungsten {

namespace GL {

QOpenGLFunctions_3_2_Core *glf = 0;

void initOpenGL()
{
    glf = (QOpenGLContext::currentContext()
        ->versionFunctions<QOpenGLFunctions_3_2_Core>());
    glf->initializeOpenGLFunctions();
}

}

}
