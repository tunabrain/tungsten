#include "TextureProperty.hpp"
#include "PropertySheet.hpp"
#include "ListProperty.hpp"

#include "materials/ConstantTexture.hpp"
#include "materials/BitmapTexture.hpp"

#include "editor/ColorPickButton.hpp"
#include "editor/TextureDisplay.hpp"
#include "editor/QtUtils.hpp"

#include "io/TextureCache.hpp"
#include "io/FileUtils.hpp"
#include "io/Scene.hpp"

#include <QtGui>

namespace Tungsten {

TextureProperty::TextureProperty(QWidget *parent, PropertySheet &sheet, std::string name,
        std::shared_ptr<Texture> value, bool allowNone, Scene *scene, TexelConversion conversion,
        std::function<bool(std::shared_ptr<Texture> &)> setter)
: _parent(parent),
  _sheet(sheet),
  _allowNone(allowNone),
  _name(std::move(name)),
  _value(std::move(value)),
  _setter(std::move(setter)),
  _scene(scene),
  _conversion(conversion),
  _currentMode(textureToMode(_value.get())),
  _texturePage(nullptr)
{
    buildTextureHeader(&sheet);
    _pageRow = sheet.rowCount();
    buildTexturePage();
}

void TextureProperty::buildTextureHeader(PropertySheet *sheet)
{
    _selectProperty = sheet->addListProperty(typeList(), modeToOption(_currentMode), _name, [this](const std::string &, int option) {
        changeMode(optionToMode(option));
        buildTexturePage();
        _setter(_value);
        return true;
    });
}

TextureProperty::TextureMode TextureProperty::textureToMode(Texture *tex) const
{
    if (ConstantTexture *c = dynamic_cast<ConstantTexture *>(tex)) {
        if (c->value().min() == c->value().max())
            return TEXTURE_SCALAR;
        else
            return TEXTURE_RGB;
    } else if (dynamic_cast<BitmapTexture *>(tex)) {
        return TEXTURE_BITMAP;
    }
    return TEXTURE_NONE;
}

std::vector<std::string> TextureProperty::typeList()
{
    std::vector<std::string> result;
    if (_allowNone)
        result.push_back("None");
    result.push_back("Scalar");
    result.push_back("RGB");
    result.push_back("Bitmap");

    return std::move(result);
}

void TextureProperty::buildTexturePage()
{
    if (_texturePage)
        _texturePage->deleteLater();

    _texturePage = new QWidget();
    PropertySheet *sheet = new PropertySheet(_texturePage);

    buildTexturePage(sheet);

    sheet->setMargin(0);

    _texturePage->setLayout(sheet);
    _sheet.addWidget(_texturePage, _pageRow, 1);
}

void TextureProperty::buildTexturePage(PropertySheet *sheet)
{
    if (_currentMode == TEXTURE_SCALAR) buildTexturePage(sheet, dynamic_cast<ConstantTexture *>(_value.get()));
    if (_currentMode == TEXTURE_RGB)    buildTexturePage(sheet, dynamic_cast<ConstantTexture *>(_value.get()));
    if (_currentMode == TEXTURE_BITMAP) buildTexturePage(sheet, dynamic_cast<BitmapTexture   *>(_value.get()));
}

void TextureProperty::buildTexturePage(PropertySheet *sheet, ConstantTexture *tex)
{
    if (_currentMode == TEXTURE_SCALAR) {
        sheet->addFloatProperty(tex->average().x(), "Value", [this, tex](float f) {
            tex->setValue(f);
            return true;
        });
    } else {
        sheet->addVectorProperty(tex->average(), "Value", false, [this, tex](Vec3f v) {
            tex->setValue(v);
            return true;
        });
    }
}

void TextureProperty::buildTexturePage(PropertySheet *sheet, BitmapTexture *tex)
{
    _gammaCorrect = tex->gammaCorrect();
    _linear = tex->linear();
    _clamp = tex->clamp();

    std::string path = tex->path()->empty() ? "" : tex->path()->absolute().asString();
    sheet->addPathProperty(path, "File", _scene->path().absolute().asString(),
            "Open bitmap...", "Image files (*.png;*.jpg;*.hdr;*.tga;*.bmp;*.psd;*.gif;*.pic;*.jpeg)",
            [this, tex](const std::string &path) {
                loadBitmap(_scene->fetchResource(path));
                return true;
    });
    sheet->addBoolProperty(tex->gammaCorrect(), "Gamma correct", [this, tex](bool b) {
        _gammaCorrect = b;
        updateBitmapFlags();
        return true;
    });
    sheet->addBoolProperty(tex->linear(), "Interpolate", [this, tex](bool b) {
        _linear = b;
        updateBitmapFlags();
        return true;
    });
    sheet->addBoolProperty(tex->clamp(), "Clamp UVs", [this, tex](bool b) {
        _clamp = b;
        updateBitmapFlags();
        return true;
    });

    _bitmapDisplay = new TextureDisplay(200, 200, _texturePage);
    _bitmapDisplay->changeTexture(tex);

    sheet->addWidget(_bitmapDisplay, sheet->rowCount(), 0, 1, 2, Qt::AlignHCenter);
}

std::shared_ptr<Texture> TextureProperty::instantiateTexture(TextureMode mode)
{
    switch (mode) {
    case TEXTURE_NONE:   return nullptr;
    case TEXTURE_SCALAR: return std::make_shared<ConstantTexture>();
    case TEXTURE_RGB:    return std::make_shared<ConstantTexture>();
    case TEXTURE_BITMAP: return std::make_shared<BitmapTexture>();
    default:             return nullptr;
    }
}

void TextureProperty::changeMode(TextureMode mode)
{
    if (mode == TEXTURE_SCALAR && _currentMode == TEXTURE_RGB) {
        ConstantTexture *tex = dynamic_cast<ConstantTexture *>(_value.get());
        tex->setValue(tex->value().x());
    } else if (!(mode == TEXTURE_RGB && _currentMode == TEXTURE_SCALAR)) {
        _value = instantiateTexture(mode);
        if (mode == TEXTURE_BITMAP)
            _value->loadResources();
    }

    _setter(_value);
    _currentMode = mode;
}

int TextureProperty::modeToOption(TextureMode mode)
{
    return static_cast<int>(mode) - (_allowNone ? 0 : 1);
}
TextureProperty::TextureMode TextureProperty::optionToMode(int option)
{
    return static_cast<TextureMode>(_allowNone ? option : option + 1);
}

void TextureProperty::loadBitmap(PathPtr path)
{
    std::shared_ptr<BitmapTexture> tex = _scene->textureCache()->fetchTexture(
            path, _conversion, _gammaCorrect, _linear, _clamp);
    tex->loadResources();

    std::shared_ptr<Texture> base(tex);
    if (_setter(base)) {
        _value = std::move(base);
        buildTexturePage();
    }
}

void TextureProperty::updateBitmapFlags()
{
    loadBitmap(dynamic_cast<BitmapTexture *>(_value.get())->path());
}

void TextureProperty::setVisible(bool visible)
{
    _selectProperty->setVisible(visible);
    _texturePage->setVisible(visible);
}

}
