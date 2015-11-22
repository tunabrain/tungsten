#ifndef OPENGL_HPP_
#define OPENGL_HPP_

#include <QOpenGLFunctions_3_2_Core>

namespace Tungsten {

namespace GL {

extern QOpenGLFunctions_3_2_Core *glf;

void initOpenGL();

}

}

#endif // OPENGL_HPP_
