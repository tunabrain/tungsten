#include "BsdfProperties.hpp"
#include "PropertyForm.hpp"

#include "primitives/Primitive.hpp"

namespace Tungsten {

void BsdfProperties::fillPropertySheet(PropertyForm *sheet, Primitive *p)
{
    if (p->numBsdfs()) {
        sheet->addBsdfProperty(p->bsdf(0), "BSDF", false, _scene, [this, p](std::shared_ptr<Bsdf> &b) {
            p->setBsdf(0, b);
            return true;
        });
    }

    sheet->setRowStretch(sheet->rowCount(), 1);
}

BsdfProperties::BsdfProperties(QWidget *proxyParent, Scene *scene, std::unordered_set<Primitive *> &selection)
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
