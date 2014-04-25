#ifndef GLDEBUG_HPP_
#define GLDEBUG_HPP_

#include "Debug.hpp"

#include <GL/glew.h>

namespace Tungsten
{

namespace GL
{

#ifndef NDEBUG
# define GLCHECK() do {uint32 e = glGetError(); ASSERT(e == 0, "glGetError() == %d  %s", e, gluErrorString(e)); } while(0)
#else
# define GLCHECK()
#endif

}

}

#endif /* GLDEBUG_HPP_ */
