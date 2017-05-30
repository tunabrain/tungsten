#ifndef BSDFPROPERTY_HPP_
#define BSDFPROPERTY_HPP_

#include "Property.hpp"

#include <functional>
#include <memory>
#include <vector>

class QComboBox;
class QWidget;

namespace Tungsten {

class PropertyForm;
class BsdfDisplay;
class Scene;
class Bsdf;

class DiffuseTransmissionBsdf;
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
        TYPE_DIFFUSE_TRANSMISSION,
        BSDF_TYPE_COUNT,
    };

    QWidget *_parent;
    PropertyForm &_sheet;
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

    void buildBsdfHeader(PropertyForm *sheet, QWidget *parent);
    void buildBsdfList();
    void buildBsdfPage();

    void buildBsdfPage(PropertyForm *sheet);
    void buildBsdfPage(PropertyForm *sheet, DiffuseTransmissionBsdf *bsdf);
    void buildBsdfPage(PropertyForm *sheet, RoughDielectricBsdf *bsdf);
    void buildBsdfPage(PropertyForm *sheet, RoughConductorBsdf *bsdf);
    void buildBsdfPage(PropertyForm *sheet, RoughPlasticBsdf *bsdf);
    void buildBsdfPage(PropertyForm *sheet, TransparencyBsdf *bsdf);
    void buildBsdfPage(PropertyForm *sheet, SmoothCoatBsdf *bsdf);
    void buildBsdfPage(PropertyForm *sheet, DielectricBsdf *bsdf);
    void buildBsdfPage(PropertyForm *sheet, ConductorBsdf *bsdf);
    void buildBsdfPage(PropertyForm *sheet, OrenNayarBsdf *bsdf);
    void buildBsdfPage(PropertyForm *sheet, RoughCoatBsdf *bsdf);
    void buildBsdfPage(PropertyForm *sheet, ThinSheetBsdf *bsdf);
    void buildBsdfPage(PropertyForm *sheet, ForwardBsdf *bsdf);
    void buildBsdfPage(PropertyForm *sheet, LambertBsdf *bsdf);
    void buildBsdfPage(PropertyForm *sheet, PlasticBsdf *bsdf);
    void buildBsdfPage(PropertyForm *sheet, MirrorBsdf *bsdf);
    void buildBsdfPage(PropertyForm *sheet, ErrorBsdf *bsdf);
    void buildBsdfPage(PropertyForm *sheet, MixedBsdf *bsdf);
    void buildBsdfPage(PropertyForm *sheet, PhongBsdf *bsdf);
    void buildBsdfPage(PropertyForm *sheet, NullBsdf *bsdf);

private slots:
    void updateBsdfDisplay();
    void pickBsdf(int idx);
    void newBsdf();

public:
    BsdfProperty(QWidget *parent, PropertyForm &sheet, std::string name, std::shared_ptr<Bsdf> value, bool nested,
            std::function<bool(std::shared_ptr<Bsdf> &)> setter, Scene *scene);

    virtual void setVisible(bool visible) override;
};

}

#endif /* BSDFPROPERTY_HPP_ */
