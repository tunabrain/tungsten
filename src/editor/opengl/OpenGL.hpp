#ifndef OPENGL_HPP_
#define OPENGL_HPP_

#ifdef __APPLE__
# include <OpenGL/gl3.h>
#else
# include <GL/glew.h>
#endif

namespace Tungsten {

namespace GL {

void initOpenGL();

}

}

#endif // OPENGL_HPP_
