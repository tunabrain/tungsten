#include "PrimitiveProperties.hpp"
#include "PropertyForm.hpp"
#include "FloatProperty.hpp"

#include "io/Scene.hpp"

namespace Tungsten {

void PrimitiveProperties::fillPropertySheet(PropertyForm *sheet, Primitive *p)
{
    sheet->addStringProperty(p->name(), "Name", [this, p](const std::string &s) {
        p->setName(s);
        emit primitiveNameChange(p);
        return true;
    });
    sheet->addTextureProperty(p->emission(), "Emission", true, _scene, TexelConversion::REQUEST_RGB, false,
        [this, p](std::shared_ptr<Texture> &tex) {
            p->setEmission(tex);
            emit triggerRedraw();
            return true;
    });
    sheet->addMediumProperty(p->intMedium(), "Interior medium", _scene, [this, p](std::shared_ptr<Medium> &m) {
        p->setIntMedium(m);
        return true;
    });
    sheet->addMediumProperty(p->extMedium(), "Exterior medium", _scene, [this, p](std::shared_ptr<Medium> &m) {
        p->setIntMedium(m);
        return true;
    });

    sheet->setRowStretch(sheet->rowCount(), 1);
}

PrimitiveProperties::PrimitiveProperties(QWidget *proxyParent, Scene *scene, std::unordered_set<Primitive *> &selection)
: QWidget(proxyParent),
  _scene(scene),
  _selection(selection)
{
    PropertyForm *sheet = new PropertyForm(this);
    setLayout(sheet);
    if (!_selection.empty())
        fillPropertySheet(sheet, *_selection.begin());
}

}
