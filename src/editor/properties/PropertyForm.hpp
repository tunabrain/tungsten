#ifndef PROPERTYSHEET_HPP_
#define PROPERTYSHEET_HPP_

#include "Property.hpp"

#include "math/Vec.hpp"

#include "io/ImageIO.hpp"

#include <QGridLayout>
#include <functional>
#include <memory>
#include <vector>

namespace Tungsten {

class TextureProperty;
class MediumProperty;
class StringProperty;
class VectorProperty;
class FloatProperty;
class PathProperty;
class ListProperty;
class BsdfProperty;
class BoolProperty;
class IntProperty;
class Texture;
class Medium;
class Scene;
class Bsdf;

class PropertyForm : public QGridLayout
{
    QWidget *_parent;

    std::vector<std::unique_ptr<Property>> _properties;

public:
    PropertyForm(QWidget *parent);

    BoolProperty *addBoolProperty(int value, std::string name, std::function<bool(bool)> setter);
    IntProperty *addIntProperty(int value, int min, int max, std::string name, std::function<bool(int)> setter);
    FloatProperty *addFloatProperty(float value, std::string name, std::function<bool(float)> setter);
    VectorProperty *addVectorProperty(Vec3f value, std::string name, bool isAbsorption,
            std::function<bool(Vec3f)> setter);
    StringProperty *addStringProperty(std::string value, std::string name,
            std::function<bool(const std::string &)> setter);
    PathProperty *addPathProperty(std::string value, std::string name,
            std::string searchDir, std::string title, std::string extensions,
            std::function<bool(const std::string &)> setter);
    ListProperty *addListProperty(std::vector<std::string> list, std::string value, std::string name,
            std::function<bool(const std::string &, int)> setter);
    ListProperty *addListProperty(std::vector<std::string> list, int index, std::string name,
            std::function<bool(const std::string &, int)> setter);
    TextureProperty *addTextureProperty(std::shared_ptr<Texture> value, std::string name, bool allowNone,
            Scene *scene, TexelConversion conversion, bool scalarGammaCorrect,
            std::function<bool(std::shared_ptr<Texture> &)> setter);
    BsdfProperty *addBsdfProperty(std::shared_ptr<Bsdf> value, std::string name, bool nested,
            Scene *scene, std::function<bool(std::shared_ptr<Bsdf> &)> setter);
    MediumProperty *addMediumProperty(std::shared_ptr<Medium> value, std::string name,
            Scene *scene, std::function<bool(std::shared_ptr<Medium> &)> setter);
};

}

#endif /* PROPERTYSHEET_HPP_ */
