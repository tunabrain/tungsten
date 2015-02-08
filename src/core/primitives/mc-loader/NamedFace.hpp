#ifndef NAMEDFACE_HPP_
#define NAMEDFACE_HPP_

#include <string>

namespace Tungsten {
namespace MinecraftLoader {

enum NamedFace {
    FACE_WEST    = 0,
    FACE_EAST    = 1,
    FACE_DOWN    = 2,
    FACE_UP      = 3,
    FACE_SOUTH   = 5,
    FACE_NORTH   = 4,
    FACE_INVALID = 6
};

static inline const char *cubeFaceToString(NamedFace f)
{
    switch (f) {
    case FACE_WEST:    return "west";
    case FACE_EAST:    return "east";
    case FACE_DOWN:    return "down";
    case FACE_UP:      return "up";
    case FACE_SOUTH:   return "south";
    case FACE_NORTH:   return "north";
    case FACE_INVALID: return "";
    }
    return "";
}

static inline NamedFace stringToCubeFace(const std::string &s)
{
    if (s == "west")
        return FACE_WEST;
    if (s == "east")
        return FACE_EAST;
    if (s == "down")
        return FACE_DOWN;
    if (s == "up")
        return FACE_UP;
    if (s == "south")
        return FACE_SOUTH;
    if (s == "north")
        return FACE_NORTH;
    return FACE_INVALID;
}

}
}

#endif /* NAMEDFACE_HPP_ */
