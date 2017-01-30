#ifndef CAMERAFACTORY_HPP_
#define CAMERAFACTORY_HPP_

#include "StringableEnum.hpp"

#include <functional>
#include <memory>

namespace Tungsten {

class Camera;

typedef StringableEnum<std::function<std::shared_ptr<Camera>()>> CameraFactory;

}

#endif /* CAMERAFACTORY_HPP_ */
