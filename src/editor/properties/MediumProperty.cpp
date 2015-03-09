#include "MediumProperty.hpp"
#include "PropertySheet.hpp"

#include "volume/Medium.hpp"

#include "io/Scene.hpp"

#include <QtGui>

namespace Tungsten {

MediumProperty::MediumProperty(QWidget *parent, PropertySheet &sheet, std::string name, std::shared_ptr<Medium> value,
        std::function<bool(std::shared_ptr<Medium> &)> setter, Scene *scene)
: _value(std::move(value)),
  _setter(std::move(setter)),
  _scene(scene)
{
    _nameLabel = new QLabel(QString::fromStdString(name + ":"), parent);

    _mediumSelector = new QComboBox(parent);

    buildMediumList();

    sheet.addWidget(_nameLabel, sheet.rowCount(), 0);
    sheet.addWidget(_mediumSelector, sheet.rowCount() - 1, 1);
}

void MediumProperty::buildMediumList()
{
    _mediumSelector->clear();

    int index = 0;
    if (!_value)
        _mediumSelector->addItem("None");
    else if (_value->unnamed())
        _mediumSelector->addItem("");

    for (const auto &m : _scene->media()) {
        if (!m->unnamed()) {
            if (m == _value)
                index = _mediumSelector->count();
            _mediumSelector->addItem(QString::fromStdString(m->name()));
        }
    }

    _mediumSelector->setCurrentIndex(index);
}

void MediumProperty::setVisible(bool visible)
{
    _nameLabel->setVisible(visible);
    _mediumSelector->setVisible(visible);
}

}
