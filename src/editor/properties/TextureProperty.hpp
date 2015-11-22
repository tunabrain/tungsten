#ifndef TEXTUREPROPERTY_HPP_
#define TEXTUREPROPERTY_HPP_

#include "Property.hpp"

#include "math/Vec.hpp"

#include "io/ImageIO.hpp"

#include <functional>
#include <memory>
#include <vector>

class QPushButton;
class QCheckBox;
class QLineEdit;
class QComboBox;
class QWidget;
class QLabel;

namespace Tungsten {

class ConstantTexture;
class BitmapTexture;

class ColorPickButton;
class TextureDisplay;
class CheckerTexture;
class PropertyForm;
class ListProperty;
class BladeTexture;
class DiskTexture;
class IesTexture;
class Texture;
class Scene;

class TextureProperty : public Property
{
    Q_OBJECT

    enum TextureMode
    {
        TEXTURE_NONE,
        TEXTURE_SCALAR,
        TEXTURE_RGB,
        TEXTURE_BITMAP,
        TEXTURE_CHECKER,
        TEXTURE_DISK,
        TEXTURE_BLADE,
        TEXTURE_IES,
        TEXTURE_COUNT
    };

    QWidget *_parent;
    PropertyForm &_sheet;
    bool _allowNone;
    std::string _name;
    std::shared_ptr<Texture> _value;
    std::function<bool(std::shared_ptr<Texture> &)> _setter;
    Scene *_scene;
    TexelConversion _conversion;

    TextureMode _currentMode;

    ListProperty *_selectProperty;
    QWidget *_texturePage;
    int _pageRow;

    bool _scalarGammaCorrect;

    bool _gammaCorrect;
    bool _linear;
    bool _clamp;

    int _resolution;

    TextureDisplay *_bitmapDisplay;

    float toLinear(float f) const;
    Vec3f toLinear(Vec3f f) const;
    float toGamma(float f) const;
    Vec3f toGamma(Vec3f f) const;

    void changeMode(TextureMode mode);

    TextureMode textureToMode(Texture *tex) const;
    std::vector<std::string> typeList();
    std::shared_ptr<Texture> instantiateTexture(TextureMode mode);
    int modeToOption(TextureMode mode);
    TextureMode optionToMode(int option);

    void buildTextureHeader(PropertyForm *sheet);
    void buildTexturePage();

    void buildTexturePage(PropertyForm *sheet);
    void buildTexturePage(PropertyForm *sheet, ConstantTexture *tex);
    void buildTexturePage(PropertyForm *sheet, BitmapTexture *tex);
    void buildTexturePage(PropertyForm *sheet, CheckerTexture *tex);
    void buildTexturePage(PropertyForm *sheet, BladeTexture *tex);
    void buildTexturePage(PropertyForm *sheet, DiskTexture *tex);
    void buildTexturePage(PropertyForm *sheet, IesTexture *tex);
    void buildTextureDisplay(PropertyForm *sheet);

    void loadBitmap(PathPtr path);
    void updateBitmapFlags();

    void loadProfile(PathPtr path);
    void updateProfileFlags();

    void updateTexture();

public:
    TextureProperty(QWidget *parent, PropertyForm &sheet, std::string name, std::shared_ptr<Texture> value,
            bool allowNone, Scene *scene, TexelConversion conversion, bool scalarGammaCorrect,
            std::function<bool(std::shared_ptr<Texture> &)> setter);

    virtual void setVisible(bool visible) override;
};

}

#endif /* TEXTUREPROPERTY_HPP_ */
