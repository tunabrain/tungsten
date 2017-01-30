#include "TextureProperty.hpp"
#include "PropertyForm.hpp"
#include "ListProperty.hpp"

#include "textures/ConstantTexture.hpp"
#include "textures/CheckerTexture.hpp"
#include "textures/BitmapTexture.hpp"
#include "textures/BladeTexture.hpp"
#include "textures/DiskTexture.hpp"
#include "textures/IesTexture.hpp"

#include "editor/ColorPickButton.hpp"
#include "editor/TextureDisplay.hpp"
#include "editor/QtUtils.hpp"

#include "io/TextureCache.hpp"
#include "io/FileUtils.hpp"
#include "io/Scene.hpp"

#include <QtWidgets>

namespace Tungsten {

TextureProperty::TextureProperty(QWidget *parent, PropertyForm &sheet, std::string name,
        std::shared_ptr<Texture> value, bool allowNone, Scene *scene, TexelConversion conversion,
        bool scalarGammaCorrect, std::function<bool(std::shared_ptr<Texture> &)> setter)
: _parent(parent),
  _sheet(sheet),
  _allowNone(allowNone),
  _name(std::move(name)),
  _value(std::move(value)),
  _setter(std::move(setter)),
  _scene(scene),
  _conversion(conversion),
  _currentMode(textureToMode(_value.get())),
  _texturePage(nullptr),
  _scalarGammaCorrect(scalarGammaCorrect)
{
    buildTextureHeader(&sheet);
    _pageRow = sheet.rowCount();
    buildTexturePage();
}

void TextureProperty::buildTextureHeader(PropertyForm *sheet)
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
    } else if (dynamic_cast<IesTexture *>(tex)) {
        return TEXTURE_IES;
    } else if (dynamic_cast<BitmapTexture *>(tex)) {
        return TEXTURE_BITMAP;
    } else if (dynamic_cast<CheckerTexture *>(tex)) {
        return TEXTURE_CHECKER;
    } else if (dynamic_cast<DiskTexture *>(tex)) {
        return TEXTURE_DISK;
    } else if (dynamic_cast<BladeTexture *>(tex)) {
        return TEXTURE_BLADE;
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
    result.push_back("Checker");
    result.push_back("Disk");
    result.push_back("Blade");
    result.push_back("IES");

    return std::move(result);
}

void TextureProperty::buildTexturePage()
{
    if (_texturePage)
        _texturePage->deleteLater();

    _texturePage = new QWidget();
    PropertyForm *sheet = new PropertyForm(_texturePage);

    buildTexturePage(sheet);

    sheet->setMargin(0);

    _texturePage->setLayout(sheet);
    _sheet.addWidget(_texturePage, _pageRow, 1);
}

void TextureProperty::buildTexturePage(PropertyForm *sheet)
{
    if (_currentMode == TEXTURE_SCALAR)  buildTexturePage(sheet, dynamic_cast<ConstantTexture *>(_value.get()));
    if (_currentMode == TEXTURE_RGB)     buildTexturePage(sheet, dynamic_cast<ConstantTexture *>(_value.get()));
    if (_currentMode == TEXTURE_BITMAP)  buildTexturePage(sheet, dynamic_cast<BitmapTexture   *>(_value.get()));
    if (_currentMode == TEXTURE_CHECKER) buildTexturePage(sheet, dynamic_cast<CheckerTexture  *>(_value.get()));
    if (_currentMode == TEXTURE_DISK)    buildTexturePage(sheet, dynamic_cast<DiskTexture     *>(_value.get()));
    if (_currentMode == TEXTURE_BLADE)   buildTexturePage(sheet, dynamic_cast<BladeTexture    *>(_value.get()));
    if (_currentMode == TEXTURE_IES)     buildTexturePage(sheet, dynamic_cast<IesTexture      *>(_value.get()));
}

void TextureProperty::buildTexturePage(PropertyForm *sheet, ConstantTexture *tex)
{
    if (_currentMode == TEXTURE_SCALAR) {
        sheet->addFloatProperty(toGamma(tex->average().x()), "Value", [this, tex](float f) {
            tex->setValue(toLinear(f));
            _setter(_value);
            return true;
        });
    } else {
        sheet->addVectorProperty(toGamma(tex->average()), "Value", false, [this, tex](Vec3f v) {
            tex->setValue(toLinear(v));
            _setter(_value);
            return true;
        });
    }
}

void TextureProperty::buildTexturePage(PropertyForm *sheet, BitmapTexture *tex)
{
    _gammaCorrect = tex->gammaCorrect();
    _linear = tex->linear();
    _clamp = tex->clamp();

    std::string path = (!tex->path() || tex->path()->empty()) ? "" : tex->path()->absolute().asString();
    sheet->addPathProperty(path, "File", _scene->path().absolute().asString(),
            "Open bitmap...", "Image files (*.png *.jpg *.hdr *.pfm *.tga *.bmp *.psd *.gif *.pic *.jpeg"
#if OPENEXR_AVAILABLE
            " *.exr"
#endif
            ")",
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

    buildTextureDisplay(sheet);
}

void TextureProperty::buildTexturePage(PropertyForm *sheet, CheckerTexture *tex)
{
    sheet->addVectorProperty(tex->onColor(), "On Color", false, [this, tex](Vec3f v) {
        tex->setOnColor(v);
        updateTexture();
        return true;
    });
    sheet->addVectorProperty(tex->offColor(), "Off Color", false, [this, tex](Vec3f v) {
        tex->setOffColor(v);
        updateTexture();
        return true;
    });
    sheet->addIntProperty(tex->resU(), 1, 9999, "Width", [this, tex](int i) {
        tex->setResU(i);
        updateTexture();
        return true;
    });
    sheet->addIntProperty(tex->resV(), 1, 9999, "Height", [this, tex](int i) {
        tex->setResV(i);
        updateTexture();
        return true;
    });

    buildTextureDisplay(sheet);
}

void TextureProperty::buildTexturePage(PropertyForm *sheet, BladeTexture *tex)
{
    sheet->addIntProperty(tex->numBlades(), 3, 999, "Number of Blades", [this, tex](int i) {
        tex->setNumBlades(i);
        updateTexture();
        return true;
    });
    sheet->addFloatProperty(Angle::radToDeg(tex->angle()), "Blade Angle", [this, tex](float f) {
        tex->setAngle(Angle::degToRad(f));
        updateTexture();
        return true;
    });

    buildTextureDisplay(sheet);
}

void TextureProperty::buildTexturePage(PropertyForm *sheet, DiskTexture * /*tex*/)
{
    buildTextureDisplay(sheet);
}

void TextureProperty::buildTexturePage(PropertyForm *sheet, IesTexture *tex)
{
    _resolution = tex->resolution();

    std::string path = (!tex->path() || tex->path()->empty()) ? "" : tex->path()->absolute().asString();
    sheet->addPathProperty(path, "File", _scene->path().absolute().asString(),
            "Open IES profile...", "IES profiles (*.ies)",
            [this, tex](const std::string &path) {
                loadProfile(_scene->fetchResource(path));
                return true;
    });
    sheet->addIntProperty(tex->resolution(), 32, 8192, "Resolution", [this, tex](int value) {
        _resolution = value;
        updateProfileFlags();
        return true;
    });

    buildTextureDisplay(sheet);
}

void TextureProperty::buildTextureDisplay(PropertyForm *sheet)
{
    _bitmapDisplay = new TextureDisplay(200, 200, _texturePage);
    _bitmapDisplay->changeTexture(_value.get());

    sheet->addWidget(_bitmapDisplay, sheet->rowCount(), 0, 1, 2, Qt::AlignHCenter);
}

std::shared_ptr<Texture> TextureProperty::instantiateTexture(TextureMode mode)
{
    switch (mode) {
    case TEXTURE_NONE:    return nullptr;
    case TEXTURE_SCALAR:  return std::make_shared<ConstantTexture>();
    case TEXTURE_RGB:     return std::make_shared<ConstantTexture>();
    case TEXTURE_BITMAP:  return std::make_shared<BitmapTexture>();
    case TEXTURE_CHECKER: return std::make_shared<CheckerTexture>();
    case TEXTURE_DISK:    return std::make_shared<DiskTexture>();
    case TEXTURE_BLADE:   return std::make_shared<BladeTexture>();
    case TEXTURE_IES:     return std::make_shared<IesTexture>();
    default:              return nullptr;
    }
}

float TextureProperty::toLinear(float f) const
{
    return _scalarGammaCorrect ? std::pow(f, 2.2f) : f;
}
Vec3f TextureProperty::toLinear(Vec3f v) const
{
    return _scalarGammaCorrect ? std::pow(v, 2.2f) : v;
}
float TextureProperty::toGamma(float f) const
{
    return _scalarGammaCorrect ? std::pow(f, 1.0f/2.2f) : f;
}
Vec3f TextureProperty::toGamma(Vec3f v) const
{
    return _scalarGammaCorrect ? std::pow(v, 1.0f/2.2f) : v;
}

void TextureProperty::changeMode(TextureMode mode)
{
    if (mode == TEXTURE_SCALAR && _currentMode == TEXTURE_RGB) {
        ConstantTexture *tex = dynamic_cast<ConstantTexture *>(_value.get());
        tex->setValue(tex->value().x());
    } else if (!(mode == TEXTURE_RGB && _currentMode == TEXTURE_SCALAR)) {
        _value = instantiateTexture(mode);
        if (mode == TEXTURE_BITMAP || mode == TEXTURE_IES)
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

void TextureProperty::loadProfile(PathPtr path)
{
    std::shared_ptr<IesTexture> tex = _scene->textureCache()->fetchIesTexture(std::move(path),
            _resolution);
    tex->loadResources();

    std::shared_ptr<Texture> base(tex);
    if (_setter(base)) {
        _value = std::move(base);
        buildTexturePage();
    }
}

void TextureProperty::updateProfileFlags()
{
    loadProfile(dynamic_cast<IesTexture *>(_value.get())->path());
}

void TextureProperty::updateTexture()
{
    _setter(_value);
    _bitmapDisplay->changeTexture(_value.get());
}

void TextureProperty::setVisible(bool visible)
{
    _selectProperty->setVisible(visible);
    _texturePage->setVisible(visible);
}

}
