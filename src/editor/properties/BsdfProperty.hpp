#ifndef BSDFPROPERTY_HPP_
#define BSDFPROPERTY_HPP_

#include "Property.hpp"

#include <functional>
#include <memory>

class QComboBox;
class QWidget;

namespace Tungsten {

class PropertySheet;
class BsdfDisplay;
class Scene;
class Bsdf;

class RoughDielectricBsdf;
class RoughConductorBsdf;
class RoughPlasticBsdf;
class TransparencyBsdf;
class SmoothCoatBsdf;
class DielectricBsdf;
class ConductorBsdf;
class OrenNayarBsdf;
class RoughCoatBsdf;
class ThinSheetBsdf;
class ForwardBsdf;
class LambertBsdf;
class PlasticBsdf;
class MirrorBsdf;
class ErrorBsdf;
class MixedBsdf;
class PhongBsdf;
class NullBsdf;

class BsdfProperty : public Property
{
    Q_OBJECT

    enum BsdfType
    {
        TYPE_CONDUCTOR,
        TYPE_DIELECTRIC,
        TYPE_ERROR,
        TYPE_FORWARD,
        TYPE_LAMBERT,
        TYPE_MIRROR,
        TYPE_MIXED,
        TYPE_NULL,
        TYPE_OREN_NAYAR,
        TYPE_PHONG,
        TYPE_PLASTIC,
        TYPE_ROUGH_COAT,
        TYPE_ROUGH_CONDUCTOR,
        TYPE_ROUGH_DIELECTRIC,
        TYPE_ROUGH_PLASTIC,
        TYPE_SMOOTH_COAT,
        TYPE_THIN_SHEET,
        TYPE_TRANSPARENCY,
        BSDF_TYPE_COUNT,
    };

    QWidget *_parent;
    PropertySheet &_sheet;
    std::string _name;
    std::shared_ptr<Bsdf> _value;
    bool _nested;
    std::function<bool(std::shared_ptr<Bsdf> &)> _setter;
    Scene *_scene;

    QComboBox *_bsdfSelector;
    std::vector<std::shared_ptr<Bsdf>> _bsdfs;
    BsdfDisplay *_display;

    QWidget *_bsdfPage;
    int _pageRow;

    BsdfType _type;

    BsdfType bsdfToType(Bsdf *bsdf) const;
    const char *typeToString(BsdfType type) const;
    std::vector<std::string> typeList() const;
    bool hasAlbedo(BsdfType type) const;
    std::shared_ptr<Bsdf> instantiateBsdf(BsdfType type) const;

    void buildBsdfHeader(PropertySheet *sheet, QWidget *parent);
    void buildBsdfList();
    void buildBsdfPage();

    void buildBsdfPage(PropertySheet *sheet);
    void buildBsdfPage(PropertySheet *sheet, RoughDielectricBsdf *bsdf);
    void buildBsdfPage(PropertySheet *sheet, RoughConductorBsdf *bsdf);
    void buildBsdfPage(PropertySheet *sheet, RoughPlasticBsdf *bsdf);
    void buildBsdfPage(PropertySheet *sheet, TransparencyBsdf *bsdf);
    void buildBsdfPage(PropertySheet *sheet, SmoothCoatBsdf *bsdf);
    void buildBsdfPage(PropertySheet *sheet, DielectricBsdf *bsdf);
    void buildBsdfPage(PropertySheet *sheet, ConductorBsdf *bsdf);
    void buildBsdfPage(PropertySheet *sheet, OrenNayarBsdf *bsdf);
    void buildBsdfPage(PropertySheet *sheet, RoughCoatBsdf *bsdf);
    void buildBsdfPage(PropertySheet *sheet, ThinSheetBsdf *bsdf);
    void buildBsdfPage(PropertySheet *sheet, ForwardBsdf *bsdf);
    void buildBsdfPage(PropertySheet *sheet, LambertBsdf *bsdf);
    void buildBsdfPage(PropertySheet *sheet, PlasticBsdf *bsdf);
    void buildBsdfPage(PropertySheet *sheet, MirrorBsdf *bsdf);
    void buildBsdfPage(PropertySheet *sheet, ErrorBsdf *bsdf);
    void buildBsdfPage(PropertySheet *sheet, MixedBsdf *bsdf);
    void buildBsdfPage(PropertySheet *sheet, PhongBsdf *bsdf);
    void buildBsdfPage(PropertySheet *sheet, NullBsdf *bsdf);

private slots:
    void updateBsdfDisplay();
    void pickBsdf(int idx);
    void newBsdf();

public:
    BsdfProperty(QWidget *parent, PropertySheet &sheet, std::string name, std::shared_ptr<Bsdf> value, bool nested,
            std::function<bool(std::shared_ptr<Bsdf> &)> setter, Scene *scene);

    virtual void setVisible(bool visible) override;
};

}

#endif /* BSDFPROPERTY_HPP_ */
