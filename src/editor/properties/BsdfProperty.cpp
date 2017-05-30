#include "BsdfProperty.hpp"
#include "VectorProperty.hpp"
#include "PropertyForm.hpp"

#include "editor/BsdfDisplay.hpp"
#include "editor/QtLambda.hpp"
#include "editor/QtUtils.hpp"

#include "bsdfs/DiffuseTransmissionBsdf.hpp"
#include "bsdfs/RoughDielectricBsdf.hpp"
#include "bsdfs/RoughConductorBsdf.hpp"
#include "bsdfs/RoughPlasticBsdf.hpp"
#include "bsdfs/TransparencyBsdf.hpp"
#include "bsdfs/DielectricBsdf.hpp"
#include "bsdfs/SmoothCoatBsdf.hpp"
#include "bsdfs/ComplexIorData.hpp"
#include "bsdfs/RoughCoatBsdf.hpp"
#include "bsdfs/ConductorBsdf.hpp"
#include "bsdfs/OrenNayarBsdf.hpp"
#include "bsdfs/ThinSheetBsdf.hpp"
#include "bsdfs/ForwardBsdf.hpp"
#include "bsdfs/LambertBsdf.hpp"
#include "bsdfs/PlasticBsdf.hpp"
#include "bsdfs/MirrorBsdf.hpp"
#include "bsdfs/ErrorBsdf.hpp"
#include "bsdfs/PhongBsdf.hpp"
#include "bsdfs/MixedBsdf.hpp"
#include "bsdfs/NullBsdf.hpp"
#include "bsdfs/Bsdf.hpp"

#include "io/Scene.hpp"

#include <QtWidgets>

namespace Tungsten {

static std::vector<std::string>   prettyDistributionList{"Beckmann", "GGX", "Phong"};
static std::vector<std::string> internalDistributionList{"beckmann", "ggx", "phong"};

BsdfProperty::BsdfProperty(QWidget *parent, PropertyForm &sheet, std::string name, std::shared_ptr<Bsdf> value, bool nested,
        std::function<bool(std::shared_ptr<Bsdf> &)> setter, Scene *scene)
: _parent(parent),
  _sheet(sheet),
  _name(std::move(name)),
  _value(std::move(value)),
  _nested(nested),
  _setter(std::move(setter)),
  _scene(scene),
  _bsdfSelector(nullptr),
  _display(nullptr),
  _bsdfPage(nullptr),
  _type(bsdfToType(_value.get()))
{
    if (_nested) {
        buildBsdfHeader(&sheet, parent);

        QFrame* line = new QFrame(parent);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        line->setStyleSheet("border: 1px solid #444;");
        line->setMaximumHeight(1);

        sheet.addWidget(line, sheet.rowCount(), 0, 1, 2);
    }

    _pageRow = sheet.rowCount();
    buildBsdfPage();
    updateBsdfDisplay();
}

BsdfProperty::BsdfType BsdfProperty::bsdfToType(Bsdf *bsdf) const
{
    if (dynamic_cast<ConductorBsdf           *>(bsdf)) return TYPE_CONDUCTOR;
    if (dynamic_cast<DielectricBsdf          *>(bsdf)) return TYPE_DIELECTRIC;
    if (dynamic_cast<ErrorBsdf               *>(bsdf)) return TYPE_ERROR;
    if (dynamic_cast<ForwardBsdf             *>(bsdf)) return TYPE_FORWARD;
    if (dynamic_cast<LambertBsdf             *>(bsdf)) return TYPE_LAMBERT;
    if (dynamic_cast<MirrorBsdf              *>(bsdf)) return TYPE_MIRROR;
    if (dynamic_cast<MixedBsdf               *>(bsdf)) return TYPE_MIXED;
    if (dynamic_cast<NullBsdf                *>(bsdf)) return TYPE_NULL;
    if (dynamic_cast<OrenNayarBsdf           *>(bsdf)) return TYPE_OREN_NAYAR;
    if (dynamic_cast<PhongBsdf               *>(bsdf)) return TYPE_PHONG;
    if (dynamic_cast<PlasticBsdf             *>(bsdf)) return TYPE_PLASTIC;
    if (dynamic_cast<RoughCoatBsdf           *>(bsdf)) return TYPE_ROUGH_COAT;
    if (dynamic_cast<RoughConductorBsdf      *>(bsdf)) return TYPE_ROUGH_CONDUCTOR;
    if (dynamic_cast<RoughDielectricBsdf     *>(bsdf)) return TYPE_ROUGH_DIELECTRIC;
    if (dynamic_cast<RoughPlasticBsdf        *>(bsdf)) return TYPE_ROUGH_PLASTIC;
    if (dynamic_cast<SmoothCoatBsdf          *>(bsdf)) return TYPE_SMOOTH_COAT;
    if (dynamic_cast<ThinSheetBsdf           *>(bsdf)) return TYPE_THIN_SHEET;
    if (dynamic_cast<TransparencyBsdf        *>(bsdf)) return TYPE_TRANSPARENCY;
    if (dynamic_cast<DiffuseTransmissionBsdf *>(bsdf)) return TYPE_DIFFUSE_TRANSMISSION;
    return TYPE_ERROR;
}
const char *BsdfProperty::typeToString(BsdfType type) const
{
    switch (type) {
    case TYPE_CONDUCTOR:            return "Conductor";
    case TYPE_DIELECTRIC:           return "Dielectric";
    case TYPE_ERROR:                return "Error";
    case TYPE_FORWARD:              return "Forward";
    case TYPE_LAMBERT:              return "Lambert";
    case TYPE_MIRROR:               return "Mirror";
    case TYPE_MIXED:                return "Mixed";
    case TYPE_NULL:                 return "Null";
    case TYPE_OREN_NAYAR:           return "Oren-Nayar";
    case TYPE_PHONG:                return "Phong";
    case TYPE_PLASTIC:              return "Plastic";
    case TYPE_ROUGH_COAT:           return "Rough Coat";
    case TYPE_ROUGH_CONDUCTOR:      return "Rough Conductor";
    case TYPE_ROUGH_DIELECTRIC:     return "Rough Dielectric";
    case TYPE_ROUGH_PLASTIC:        return "Rough Plastic";
    case TYPE_SMOOTH_COAT:          return "Smooth Coat";
    case TYPE_THIN_SHEET:           return "Thin Sheet";
    case TYPE_TRANSPARENCY:         return "Transparency";
    case TYPE_DIFFUSE_TRANSMISSION: return "Diffuse Transmission";
    default:                    return "";
    }
}

std::vector<std::string> BsdfProperty::typeList() const
{
    std::vector<std::string> result;
    for (int i = 0; i < BSDF_TYPE_COUNT; ++i)
        result.emplace_back(typeToString(static_cast<BsdfType>(i)));

    return std::move(result);
}

bool BsdfProperty::hasAlbedo(BsdfType type) const
{
    switch (type) {
    case TYPE_ERROR:
    case TYPE_FORWARD:
    case TYPE_NULL:
    case TYPE_ROUGH_COAT:
    case TYPE_SMOOTH_COAT:
    case TYPE_THIN_SHEET:
    case TYPE_TRANSPARENCY:
        return false;
    default:
        return true;
    }
}

std::shared_ptr<Bsdf> BsdfProperty::instantiateBsdf(BsdfType type) const
{
    switch (type) {
    case TYPE_CONDUCTOR:            return std::make_shared<ConductorBsdf>();
    case TYPE_DIELECTRIC:           return std::make_shared<DielectricBsdf>();
    case TYPE_ERROR:                return std::make_shared<ErrorBsdf>();
    case TYPE_FORWARD:              return std::make_shared<ForwardBsdf>();
    case TYPE_LAMBERT:              return std::make_shared<LambertBsdf>();
    case TYPE_MIRROR:               return std::make_shared<MirrorBsdf>();
    case TYPE_MIXED:                return std::make_shared<MixedBsdf>();
    case TYPE_NULL:                 return std::make_shared<NullBsdf>();
    case TYPE_OREN_NAYAR:           return std::make_shared<OrenNayarBsdf>();
    case TYPE_PHONG:                return std::make_shared<PhongBsdf>();
    case TYPE_PLASTIC:              return std::make_shared<PlasticBsdf>();
    case TYPE_ROUGH_COAT:           return std::make_shared<RoughCoatBsdf>();
    case TYPE_ROUGH_CONDUCTOR:      return std::make_shared<RoughConductorBsdf>();
    case TYPE_ROUGH_DIELECTRIC:     return std::make_shared<RoughDielectricBsdf>();
    case TYPE_ROUGH_PLASTIC:        return std::make_shared<RoughPlasticBsdf>();
    case TYPE_SMOOTH_COAT:          return std::make_shared<SmoothCoatBsdf>();
    case TYPE_THIN_SHEET:           return std::make_shared<ThinSheetBsdf>();
    case TYPE_TRANSPARENCY:         return std::make_shared<TransparencyBsdf>();
    case TYPE_DIFFUSE_TRANSMISSION: return std::make_shared<DiffuseTransmissionBsdf>();
    default:                    return nullptr;
    }
}

void BsdfProperty::buildBsdfHeader(PropertyForm *sheet, QWidget *parent)
{
    _bsdfSelector = new QComboBox(parent);
    connect(_bsdfSelector, SIGNAL(activated(int)), this, SLOT(pickBsdf(int)));

    QPushButton *addButton = minimumSizeButton("+", parent);
    connect(addButton, SIGNAL(clicked()), this, SLOT(newBsdf()));

    QBoxLayout *horz = new QBoxLayout(QBoxLayout::LeftToRight);
    horz->addWidget(_bsdfSelector, 1);
    horz->addWidget(addButton, 0);

    buildBsdfList();

    sheet->addWidget(new QLabel(QString::fromStdString(_name + ":"), parent), sheet->rowCount(), 0);
    sheet->addLayout(horz, sheet->rowCount() - 1, 1);
}

void BsdfProperty::buildBsdfList()
{
    _bsdfSelector->clear();
    _bsdfs.clear();

    int index = 0;
    if (_value->unnamed()) {
        _bsdfSelector->addItem("");
        _bsdfs.push_back(_value);
    }

    for (const auto &b : _scene->bsdfs()) {
        if (!b->unnamed()) {
            if (b == _value)
                index = _bsdfSelector->count();
            _bsdfSelector->addItem(QString::fromStdString(b->name()));
            _bsdfs.push_back(b);
        }
    }

    _bsdfSelector->setCurrentIndex(index);
}

void BsdfProperty::buildBsdfPage()
{
    if (_bsdfPage)
        _bsdfPage->deleteLater();
    if (_display)
        delete _display;

    _bsdfPage = new QWidget();
    PropertyForm *sheet = new PropertyForm(_bsdfPage);

    _display = new BsdfDisplay(200, 200, _bsdfPage);
    sheet->addWidget(_display, 0, 0, 1, 2, Qt::AlignHCenter);

    if (!_nested)
        buildBsdfHeader(sheet, _bsdfPage);

    sheet->addListProperty(typeList(), static_cast<int>(_type), "Type", [this](const std::string &, int idx) {
        BsdfType type = static_cast<BsdfType>(idx);
        std::shared_ptr<Bsdf> bsdf = instantiateBsdf(type);
        bsdf->setName(_value->name());
        if (hasAlbedo(_type) && hasAlbedo(type))
            bsdf->setAlbedo(_value->albedo());
        if (_setter(bsdf)) {
            for (auto &p : _scene->primitives())
                for (int i = 0; i < p->numBsdfs(); ++i)
                    if (p->bsdf(i) == _value)
                        p->setBsdf(i, bsdf);

            _value->setName("");
            _scene->deleteBsdfs(std::unordered_set<Bsdf *>{_value.get()});

            _type = type;
            _value = std::move(bsdf);
            _scene->addBsdf(_value);
            buildBsdfList();
            buildBsdfPage();
            updateBsdfDisplay();
        }

        return true;
    });
    sheet->addStringProperty(_value->name(), "Name", [this](const std::string &name) {
        bool wasEmpty = _value->name().empty();
        _value->setName(name);
        if (!wasEmpty && name.empty())
            _scene->deleteBsdfs(std::unordered_set<Bsdf *>{_value.get()});
        else if (wasEmpty && !name.empty())
            _scene->addBsdf(_value);
        buildBsdfList();
        return true;
    });
    if (hasAlbedo(_type)) {
        sheet->addTextureProperty(_value->albedo(), "Albedo", false, _scene, TexelConversion::REQUEST_RGB, true,
            [this](std::shared_ptr<Texture> &t) {
                _value->setAlbedo(t);
                updateBsdfDisplay();
                return true;
        });
    }
    sheet->addTextureProperty(_value->bump(), "Bump map", true, _scene, TexelConversion::REQUEST_AVERAGE, false,
        [this](std::shared_ptr<Texture> &bump) {
            _value->setBump(bump);
            updateBsdfDisplay();
            return true;
    });

    buildBsdfPage(sheet);

    sheet->setMargin(0);
    if (_nested)
        sheet->setContentsMargins(20, 0, 0, 0);

    _bsdfPage->setLayout(sheet);
    _sheet.addWidget(_bsdfPage, _pageRow, 0, 1, 2);
}

void BsdfProperty::buildBsdfPage(PropertyForm *sheet)
{
    switch (_type) {
    case TYPE_CONDUCTOR:            buildBsdfPage(sheet, dynamic_cast<ConductorBsdf           *>(_value.get())); break;
    case TYPE_DIELECTRIC:           buildBsdfPage(sheet, dynamic_cast<DielectricBsdf          *>(_value.get())); break;
    case TYPE_ERROR:                buildBsdfPage(sheet, dynamic_cast<ErrorBsdf               *>(_value.get())); break;
    case TYPE_FORWARD:              buildBsdfPage(sheet, dynamic_cast<ForwardBsdf             *>(_value.get())); break;
    case TYPE_LAMBERT:              buildBsdfPage(sheet, dynamic_cast<LambertBsdf             *>(_value.get())); break;
    case TYPE_MIRROR:               buildBsdfPage(sheet, dynamic_cast<MirrorBsdf              *>(_value.get())); break;
    case TYPE_MIXED:                buildBsdfPage(sheet, dynamic_cast<MixedBsdf               *>(_value.get())); break;
    case TYPE_NULL:                 buildBsdfPage(sheet, dynamic_cast<NullBsdf                *>(_value.get())); break;
    case TYPE_OREN_NAYAR:           buildBsdfPage(sheet, dynamic_cast<OrenNayarBsdf           *>(_value.get())); break;
    case TYPE_PHONG:                buildBsdfPage(sheet, dynamic_cast<PhongBsdf               *>(_value.get())); break;
    case TYPE_PLASTIC:              buildBsdfPage(sheet, dynamic_cast<PlasticBsdf             *>(_value.get())); break;
    case TYPE_ROUGH_COAT:           buildBsdfPage(sheet, dynamic_cast<RoughCoatBsdf           *>(_value.get())); break;
    case TYPE_ROUGH_CONDUCTOR:      buildBsdfPage(sheet, dynamic_cast<RoughConductorBsdf      *>(_value.get())); break;
    case TYPE_ROUGH_DIELECTRIC:     buildBsdfPage(sheet, dynamic_cast<RoughDielectricBsdf     *>(_value.get())); break;
    case TYPE_ROUGH_PLASTIC:        buildBsdfPage(sheet, dynamic_cast<RoughPlasticBsdf        *>(_value.get())); break;
    case TYPE_SMOOTH_COAT:          buildBsdfPage(sheet, dynamic_cast<SmoothCoatBsdf          *>(_value.get())); break;
    case TYPE_THIN_SHEET:           buildBsdfPage(sheet, dynamic_cast<ThinSheetBsdf           *>(_value.get())); break;
    case TYPE_TRANSPARENCY:         buildBsdfPage(sheet, dynamic_cast<TransparencyBsdf        *>(_value.get())); break;
    case TYPE_DIFFUSE_TRANSMISSION: buildBsdfPage(sheet, dynamic_cast<DiffuseTransmissionBsdf *>(_value.get())); break;
    default: break;
    }
}

static void addComplexIorProperty(QWidget *parent, PropertyForm *sheet, const std::string &material, Vec3f eta, Vec3f k,
        std::function<void(const std::string &)> materialSetter, std::function<bool(Vec3f)> etaSetter,
        std::function<bool(Vec3f)> kSetter)
{
    QComboBox *presets = new QComboBox(parent);
    int index = 0;
    for (int i = 0; i < ComplexIorList::ComplexIorCount; ++i) {
        presets->addItem(QString::fromStdString(ComplexIorList::complexIorList[i].name));
        if (ComplexIorList::complexIorList[i].name == material)
            index = i;
    }
    presets->setCurrentIndex(index);

    QBoxLayout *horz = new QBoxLayout(QBoxLayout::LeftToRight);
    horz->addWidget(new QLabel("Eta/K Presets:", parent), 0);
    horz->addWidget(presets, 1);

    VectorProperty *etaProp = sheet->addVectorProperty(eta, "Eta", false, std::move(etaSetter));
    VectorProperty *kProp = sheet->addVectorProperty(k, "K", false, std::move(kSetter));
    sheet->addLayout(horz, sheet->rowCount(), 1);

    QtLambda *lambdaSlot = new QtLambda(parent, [materialSetter, etaProp, kProp, presets]() {
        int index = presets->currentIndex();
        materialSetter(ComplexIorList::complexIorList[index].name);
        Vec3f eta = ComplexIorList::complexIorList[index].eta;
        Vec3f k = ComplexIorList::complexIorList[index].k;
        etaProp->setValue(eta);
        kProp->setValue(k);
    });

    QObject::connect(presets, SIGNAL(activated(int)), lambdaSlot, SLOT(call()));
}

void BsdfProperty::buildBsdfPage(PropertyForm *sheet, DiffuseTransmissionBsdf *bsdf)
{
    sheet->addFloatProperty(bsdf->transmittance(), "Transmittance", [this, bsdf](float t) {
        bsdf->setTransmittance(t);
        updateBsdfDisplay();
        return true;
    });
}

void BsdfProperty::buildBsdfPage(PropertyForm *sheet, RoughDielectricBsdf *bsdf)
{
    sheet->addFloatProperty(bsdf->ior(), "IOR", [this, bsdf](float f) {
        bsdf->setIor(f);
        updateBsdfDisplay();
        return true;
    });
    sheet->addBoolProperty(bsdf->enableTransmission(), "Refraction", [this, bsdf](bool b) {
        bsdf->setEnableTransmission(b);
        updateBsdfDisplay();
        return true;
    });
    sheet->addListProperty(prettyDistributionList, bsdf->distributionName(),
        "Distribution", [this, bsdf](const std::string &/*name*/, int idx) {
            bsdf->setDistributionName(internalDistributionList[idx]);
            updateBsdfDisplay();
            return true;
    });
    sheet->addTextureProperty(bsdf->roughness(), "Roughness", false, _scene, TexelConversion::REQUEST_AVERAGE, false,
        [this, bsdf](std::shared_ptr<Texture> &tex) {
            bsdf->setRoughness(tex);
            updateBsdfDisplay();
            return true;
    });
}

void BsdfProperty::buildBsdfPage(PropertyForm *sheet, RoughConductorBsdf *bsdf)
{
    addComplexIorProperty(_bsdfPage, sheet, bsdf->materialName(), bsdf->eta(), bsdf->k(),
        [this, bsdf](const std::string &name) {
            bsdf->setMaterialName(name);
            updateBsdfDisplay();
        },
        [this, bsdf](Vec3f eta) {
            bsdf->setEta(eta);
            updateBsdfDisplay();
            return true;
        },
        [this, bsdf](Vec3f k) {
            bsdf->setK(k);
            updateBsdfDisplay();
            return true;
        }
    );
    sheet->addListProperty(prettyDistributionList, bsdf->distributionName(),
        "Distribution", [this, bsdf](const std::string &/*name*/, int idx) {
            bsdf->setDistributionName(internalDistributionList[idx]);
            updateBsdfDisplay();
            return true;
    });
    sheet->addTextureProperty(bsdf->roughness(), "Roughness", false, _scene, TexelConversion::REQUEST_AVERAGE, false,
        [this, bsdf](std::shared_ptr<Texture> &tex) {
            bsdf->setRoughness(tex);
            updateBsdfDisplay();
            return true;
    });
}

void BsdfProperty::buildBsdfPage(PropertyForm *sheet, RoughPlasticBsdf *bsdf)
{
    sheet->addFloatProperty(bsdf->ior(), "IOR", [this, bsdf](float f) {
        bsdf->setIor(f);
        updateBsdfDisplay();
        return true;
    });
    sheet->addVectorProperty(bsdf->sigmaA(), "Absorption", true, [this, bsdf](Vec3f v) {
        bsdf->setSigmaA(v);
        updateBsdfDisplay();
        return true;
    });
    sheet->addFloatProperty(bsdf->thickness(), "Thickness", [this, bsdf](float f) {
        bsdf->setThickness(f);
        updateBsdfDisplay();
        return true;
    });
    sheet->addListProperty(prettyDistributionList, bsdf->distributionName(),
        "Distribution", [this, bsdf](const std::string &/*name*/, int idx) {
            bsdf->setDistributionName(internalDistributionList[idx]);
            updateBsdfDisplay();
            return true;
    });
    sheet->addTextureProperty(bsdf->roughness(), "Roughness", false, _scene, TexelConversion::REQUEST_AVERAGE, false,
        [this, bsdf](std::shared_ptr<Texture> &tex) {
            bsdf->setRoughness(tex);
            updateBsdfDisplay();
            return true;
    });
}

void BsdfProperty::buildBsdfPage(PropertyForm *sheet, TransparencyBsdf *bsdf)
{
    sheet->addTextureProperty(bsdf->opacity(), "Opacity", false, _scene, TexelConversion::REQUEST_AUTO, true,
        [this, bsdf](std::shared_ptr<Texture> &tex) {
            bsdf->setOpacity(tex);
            updateBsdfDisplay();
            return true;
    });
    sheet->addBsdfProperty(bsdf->base(), "Base", true, _scene, [this, bsdf](std::shared_ptr<Bsdf> &b) {
        if (b.get() == bsdf)
            return false;
        bsdf->setBase(b);
        updateBsdfDisplay();
        return true;
    });
}

void BsdfProperty::buildBsdfPage(PropertyForm *sheet, SmoothCoatBsdf *bsdf)
{
    sheet->addFloatProperty(bsdf->ior(), "IOR", [this, bsdf](float f) {
        bsdf->setIor(f);
        updateBsdfDisplay();
        return true;
    });
    sheet->addVectorProperty(bsdf->sigmaA(), "Absorption", true, [this, bsdf](Vec3f v) {
        bsdf->setSigmaA(v);
        updateBsdfDisplay();
        return true;
    });
    sheet->addFloatProperty(bsdf->thickness(), "Thickness", [this, bsdf](float f) {
        bsdf->setThickness(f);
        updateBsdfDisplay();
        return true;
    });
    sheet->addBsdfProperty(bsdf->substrate(), "Substrate", true, _scene, [this, bsdf](std::shared_ptr<Bsdf> &b) {
        if (b.get() == bsdf)
            return false;
        bsdf->setSubstrate(b);
        updateBsdfDisplay();
        return true;
    });
}

void BsdfProperty::buildBsdfPage(PropertyForm *sheet, DielectricBsdf *bsdf)
{
    sheet->addFloatProperty(bsdf->ior(), "IOR", [this, bsdf](float f) {
        bsdf->setIor(f);
        updateBsdfDisplay();
        return true;
    });
    sheet->addBoolProperty(bsdf->enableTransmission(), "Refraction", [this, bsdf](bool b) {
        bsdf->setEnableTransmission(b);
        updateBsdfDisplay();
        return true;
    });
}

void BsdfProperty::buildBsdfPage(PropertyForm *sheet, ConductorBsdf *bsdf)
{
    addComplexIorProperty(_bsdfPage, sheet, bsdf->materialName(), bsdf->eta(), bsdf->k(),
        [this, bsdf](const std::string &name) {
            bsdf->setMaterialName(name);
            updateBsdfDisplay();
        },
        [this, bsdf](Vec3f eta) {
            bsdf->setEta(eta);
            updateBsdfDisplay();
            return true;
        },
        [this, bsdf](Vec3f k) {
            bsdf->setK(k);
            updateBsdfDisplay();
            return true;
        }
    );
}

void BsdfProperty::buildBsdfPage(PropertyForm *sheet, OrenNayarBsdf *bsdf)
{
    sheet->addTextureProperty(bsdf->roughness(), "Roughness", false, _scene, TexelConversion::REQUEST_AVERAGE, false,
        [this, bsdf](std::shared_ptr<Texture> &tex) {
            bsdf->setRoughness(tex);
            updateBsdfDisplay();
            return true;
    });
}

void BsdfProperty::buildBsdfPage(PropertyForm *sheet, RoughCoatBsdf *bsdf)
{
    sheet->addFloatProperty(bsdf->ior(), "IOR", [this, bsdf](float f) {
        bsdf->setIor(f);
        updateBsdfDisplay();
        return true;
    });
    sheet->addVectorProperty(bsdf->sigmaA(), "Absorption", true, [this, bsdf](Vec3f v) {
        bsdf->setSigmaA(v);
        updateBsdfDisplay();
        return true;
    });
    sheet->addFloatProperty(bsdf->thickness(), "Thickness", [this, bsdf](float f) {
        bsdf->setThickness(f);
        updateBsdfDisplay();
        return true;
    });
    sheet->addListProperty(prettyDistributionList, bsdf->distributionName(),
        "Distribution", [this, bsdf](const std::string &/*name*/, int idx) {
            bsdf->setDistributionName(internalDistributionList[idx]);
            updateBsdfDisplay();
            return true;
    });
    sheet->addTextureProperty(bsdf->roughness(), "Roughness", false, _scene, TexelConversion::REQUEST_AVERAGE, false,
        [this, bsdf](std::shared_ptr<Texture> &tex) {
            bsdf->setRoughness(tex);
            updateBsdfDisplay();
            return true;
    });
    sheet->addBsdfProperty(bsdf->substrate(), "Substrate", true, _scene, [this, bsdf](std::shared_ptr<Bsdf> &b) {
        if (b.get() == bsdf)
            return false;
        bsdf->setSubstrate(b);
        updateBsdfDisplay();
        return true;
    });
}

void BsdfProperty::buildBsdfPage(PropertyForm *sheet, ThinSheetBsdf *bsdf)
{
    sheet->addFloatProperty(bsdf->ior(), "IOR", [this, bsdf](float f) {
        bsdf->setIor(f);
        updateBsdfDisplay();
        return true;
    });
    sheet->addVectorProperty(bsdf->sigmaA(), "Absorption", true, [this, bsdf](Vec3f v) {
        bsdf->setSigmaA(v);
        updateBsdfDisplay();
        return true;
    });
    sheet->addTextureProperty(bsdf->thickness(), "Thickness", false, _scene, TexelConversion::REQUEST_AVERAGE, false,
        [this, bsdf](std::shared_ptr<Texture> &tex) {
            bsdf->setThickness(tex);
            updateBsdfDisplay();
            return true;
    });
    sheet->addBoolProperty(bsdf->enableInterference(), "Interference", [this, bsdf](bool b) {
        bsdf->setEnableInterference(b);
        updateBsdfDisplay();
        return true;
    });
}

void BsdfProperty::buildBsdfPage(PropertyForm * /*sheet*/, ForwardBsdf * /*bsdf*/)
{
}

void BsdfProperty::buildBsdfPage(PropertyForm * /*sheet*/, LambertBsdf * /*bsdf*/)
{
}

void BsdfProperty::buildBsdfPage(PropertyForm *sheet, PlasticBsdf *bsdf)
{
    sheet->addFloatProperty(bsdf->ior(), "IOR", [this, bsdf](float f) {
        bsdf->setIor(f);
        updateBsdfDisplay();
        return true;
    });
    sheet->addVectorProperty(bsdf->sigmaA(), "Absorption", true, [this, bsdf](Vec3f v) {
        bsdf->setSigmaA(v);
        updateBsdfDisplay();
        return true;
    });
    sheet->addFloatProperty(bsdf->thickness(), "Thickness", [this, bsdf](float f) {
        bsdf->setThickness(f);
        updateBsdfDisplay();
        return true;
    });
}

void BsdfProperty::buildBsdfPage(PropertyForm * /*sheet*/, MirrorBsdf * /*bsdf*/)
{
}

void BsdfProperty::buildBsdfPage(PropertyForm * /*sheet*/, ErrorBsdf * /*bsdf*/)
{
}

void BsdfProperty::buildBsdfPage(PropertyForm *sheet, MixedBsdf *bsdf)
{
    sheet->addTextureProperty(bsdf->ratio(), "Ratio", false, _scene, TexelConversion::REQUEST_AVERAGE, false,
        [this, bsdf](std::shared_ptr<Texture> &tex) {
            bsdf->setRatio(tex);
            updateBsdfDisplay();
            return true;
    });
    sheet->addBsdfProperty(bsdf->bsdf0(), "1st BSDF", true, _scene, [this, bsdf](std::shared_ptr<Bsdf> &b) {
        if (b.get() == bsdf)
            return false;
        bsdf->setBsdf0(b);
        updateBsdfDisplay();
        return true;
    });
    sheet->addBsdfProperty(bsdf->bsdf1(), "2nd BSDF", true, _scene, [this, bsdf](std::shared_ptr<Bsdf> &b) {
        if (b.get() == bsdf)
            return false;
        bsdf->setBsdf1(b);
        updateBsdfDisplay();
        return true;
    });
}

void BsdfProperty::buildBsdfPage(PropertyForm *sheet, PhongBsdf *bsdf)
{
    sheet->addFloatProperty(bsdf->exponent(), "Exponent", [this, bsdf](float f) {
        bsdf->setExponent(f);
        updateBsdfDisplay();
        return true;
    });
    sheet->addFloatProperty(bsdf->diffuseRatio(), "Diffuse Ratio", [this, bsdf](float f) {
        bsdf->setDiffuseRatio(f);
        updateBsdfDisplay();
        return true;
    });
}

void BsdfProperty::buildBsdfPage(PropertyForm * /*sheet*/, NullBsdf * /*bsdf*/)
{
}

void BsdfProperty::updateBsdfDisplay()
{
    _display->changeBsdf(_value);
}

void BsdfProperty::pickBsdf(int idx)
{
    if (_bsdfs[idx] != _value && _setter(_bsdfs[idx])) {
        _value = _bsdfs[idx];
        _type = bsdfToType(_value.get());
        buildBsdfList();
        buildBsdfPage();
        updateBsdfDisplay();
    }
}

void BsdfProperty::newBsdf()
{
    std::shared_ptr<Bsdf> bsdf = std::make_shared<LambertBsdf>();
    if (_setter(bsdf)) {
        _value = std::move(bsdf);
        _scene->addBsdf(_value);
        _type = bsdfToType(_value.get());
        buildBsdfList();
        buildBsdfPage();
        updateBsdfDisplay();
    }
}

void BsdfProperty::setVisible(bool visible)
{
    _bsdfPage->setVisible(visible);
}

}
