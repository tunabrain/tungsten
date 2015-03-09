#include "PrimitiveProperties.hpp"
#include "PropertySheet.hpp"
#include "FloatProperty.hpp"

#include "io/Scene.hpp"

namespace Tungsten {

void PrimitiveProperties::fillPropertySheet(PropertySheet *sheet, Primitive *p)
{
    sheet->addStringProperty(p->name(), "Name", [this, p](const std::string &s) {
        p->setName(s);
        emit primitiveNameChange(p);
        return true;
    });
    sheet->addTextureProperty(p->emission(), "Emission", true, _scene, TexelConversion::REQUEST_RGB,
        [this, p](std::shared_ptr<Texture> &tex) {
            p->setEmission(tex);
            emit triggerRedraw();
            return true;
    });
    FloatProperty *bumpStrength = sheet->addFloatProperty(p->bumpStrength(), "Bump strength", [this, p](float s) {
        p->setBumpStrength(s);
        return true;
    });
    sheet->addTextureProperty(p->bump(), "Bump map", true, _scene, TexelConversion::REQUEST_AVERAGE,
        [this, p, bumpStrength](std::shared_ptr<Texture> &bump) {
            p->setBump(bump);
            bumpStrength->setVisible(bump != nullptr);
            return true;
    });
    if (!p->bump())
        bumpStrength->setVisible(false);

    sheet->setRowStretch(sheet->rowCount(), 1);
}

PrimitiveProperties::PrimitiveProperties(QWidget *proxyParent, Scene *scene, std::unordered_set<Primitive *> &selection)
: QWidget(proxyParent),
  _scene(scene),
  _selection(selection)
{
    PropertySheet *sheet = new PropertySheet(this);
    setLayout(sheet);
    if (!_selection.empty())
        fillPropertySheet(sheet, *_selection.begin());
}

}
