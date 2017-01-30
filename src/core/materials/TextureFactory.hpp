#ifndef TEXTUREFACTORY_HPP_
#define TEXTUREFACTORY_HPP_

#include "StringableEnum.hpp"

#include <functional>
#include <memory>

namespace Tungsten {

class Texture;

typedef StringableEnum<std::function<std::shared_ptr<Texture>()>> TextureFactory;

}

#endif /* TEXTUREFACTORY_HPP_ */
