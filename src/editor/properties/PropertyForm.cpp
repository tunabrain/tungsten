#include "PropertyForm.hpp"
#include "TextureProperty.hpp"
#include "MediumProperty.hpp"
#include "VectorProperty.hpp"
#include "StringProperty.hpp"
#include "FloatProperty.hpp"
#include "PathProperty.hpp"
#include "ListProperty.hpp"
#include "BsdfProperty.hpp"
#include "BoolProperty.hpp"
#include "IntProperty.hpp"

#include <QtWidgets>

namespace Tungsten {

PropertyForm::PropertyForm(QWidget *parent)
: _parent(parent)
{
    setColumnStretch(0, 0);
    setColumnStretch(1, 1);
}

BoolProperty *PropertyForm::addBoolProperty(int value, std::string name, std::function<bool(bool)> setter)
{
    BoolProperty *property = new BoolProperty(_parent, *this, std::move(name), value, std::move(setter));
    _properties.emplace_back(property);
    return property;
}

IntProperty *PropertyForm::addIntProperty(int value, int min, int max, std::string name, std::function<bool(int)> setter)
{
    IntProperty *property = new IntProperty(_parent, *this, std::move(name), value, min, max, std::move(setter));
    _properties.emplace_back(property);
    return property;
}

FloatProperty *PropertyForm::addFloatProperty(float value, std::string name, std::function<bool(float)> setter)
{
    FloatProperty *property = new FloatProperty(_parent, *this, std::move(name), value, std::move(setter));
    _properties.emplace_back(property);
    return property;
}

VectorProperty *PropertyForm::addVectorProperty(Vec3f value, std::string name, bool isAbsorption,
        std::function<bool(Vec3f)> setter)
{
    VectorProperty *property = new VectorProperty(_parent, *this, std::move(name), value, isAbsorption, std::move(setter));
    _properties.emplace_back(property);
    return property;
}

StringProperty *PropertyForm::addStringProperty(std::string value, std::string name, std::function<bool(const std::string &)> setter)
{
    StringProperty *property = new StringProperty(_parent, *this, std::move(name),
            std::move(value), std::move(setter));
    _properties.emplace_back(property);
    return property;
}

PathProperty *PropertyForm::addPathProperty(std::string value, std::string name,
        std::string searchDir, std::string title, std::string extensions,
        std::function<bool(const std::string &)> setter)
{
    PathProperty *property = new PathProperty(_parent, *this, std::move(name),
            std::move(value), std::move(searchDir), std::move(title), std::move(extensions),
            std::move(setter));
    _properties.emplace_back(property);
    return property;
}

ListProperty *PropertyForm::addListProperty(std::vector<std::string> list, std::string value, std::string name,
        std::function<bool(const std::string &, int)> setter)
{
    ListProperty *property = new ListProperty(_parent, *this, std::move(name),
            std::move(list), std::move(value), std::move(setter));
    _properties.emplace_back(property);
    return property;
}

ListProperty *PropertyForm::addListProperty(std::vector<std::string> list, int index, std::string name,
        std::function<bool(const std::string &, int)> setter)
{
    ListProperty *property = new ListProperty(_parent, *this, std::move(name),
            std::move(list), index, std::move(setter));
    _properties.emplace_back(property);
    return property;
}

TextureProperty *PropertyForm::addTextureProperty(std::shared_ptr<Texture> value, std::string name,
        bool allowNone, Scene *scene, TexelConversion conversion,
        bool scalarGammaCorrect, std::function<bool(std::shared_ptr<Texture> &)> setter)
{
    TextureProperty *property = new TextureProperty(_parent, *this, std::move(name),
            std::move(value), allowNone, scene, conversion, scalarGammaCorrect, std::move(setter));
    _properties.emplace_back(property);
    return property;
}

BsdfProperty *PropertyForm::addBsdfProperty(std::shared_ptr<Bsdf> value, std::string name, bool nested,
        Scene *scene, std::function<bool(std::shared_ptr<Bsdf> &)> setter)
{
    BsdfProperty *property = new BsdfProperty(_parent, *this, name, std::move(value), nested, std::move(setter), scene);
    _properties.emplace_back(property);
    return property;
}

MediumProperty *PropertyForm::addMediumProperty(std::shared_ptr<Medium> value, std::string name,
        Scene *scene, std::function<bool(std::shared_ptr<Medium> &)> setter)
{
    MediumProperty *property = new MediumProperty(_parent, *this, name, std::move(value), std::move(setter), scene);
    _properties.emplace_back(property);
    return property;
}

}
