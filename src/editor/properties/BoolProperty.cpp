#include "BoolProperty.hpp"
#include "PropertyForm.hpp"

#include <QtWidgets>

namespace Tungsten {

BoolProperty::BoolProperty(QWidget *parent, PropertyForm &sheet, std::string name, int value,
        std::function<bool(bool)> setter)
: _value(value),
  _setter(std::move(setter))
{
    _checkbox = new QCheckBox(name.c_str(), parent);
    _checkbox->setChecked(_value);
    connect(_checkbox, SIGNAL(stateChanged(int)), this, SLOT(stateChanged(int)));

    sheet.addWidget(_checkbox, sheet.rowCount(), 0, 1, 2);
}

void BoolProperty::setVisible(bool visible)
{
    _checkbox->setVisible(visible);
}

void BoolProperty::stateChanged(int state)
{
    if (state != _value) {
        if (!_setter(state))
            _checkbox->setChecked(_value);
        else
            _value = state;
    }
}

}
