#include "TextureFactory.hpp"

#include "ConstantTexture.hpp"
#include "CheckerTexture.hpp"
#include "BladeTexture.hpp"
#include "DiskTexture.hpp"
#include "IesTexture.hpp"

namespace Tungsten {

DEFINE_STRINGABLE_ENUM(TextureFactory, "texture", ({
    {"bitmap", std::make_shared<BitmapTexture>},
    {"constant", std::make_shared<ConstantTexture>},
    {"checker", std::make_shared<CheckerTexture>},
    {"disk", std::make_shared<DiskTexture>},
    {"blade", std::make_shared<BladeTexture>},
    {"ies", std::make_shared<IesTexture>},
}))

}
