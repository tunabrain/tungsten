#include "IntProperty.hpp"
#include "PropertyForm.hpp"

#include <QtWidgets>

namespace Tungsten {

IntProperty::IntProperty(QWidget *parent, PropertyForm &sheet, std::string name, int value, int min, int max,
        std::function<bool(int)> setter)
: _value(value),
  _setter(std::move(setter))
{
    _nameLabel = new QLabel(QString::fromStdString(name + ":"), parent);
    _spinner = new QSpinBox(parent);
    _spinner->setMinimum(min);
    _spinner->setMaximum(max);
    _spinner->setValue(value);
    connect(_spinner, SIGNAL(valueChanged(int)), this, SLOT(setValue(int)));

    sheet.addWidget(_nameLabel, sheet.rowCount(), 0);
    sheet.addWidget(_spinner, sheet.rowCount() - 1, 1);
}

void IntProperty::setValue(int newValue)
{
    if (newValue != _value) {
        if (!_setter(newValue))
            _spinner->setValue(_value);
        else
            _value = newValue;
    }
}

void IntProperty::setVisible(bool visible)
{
    _nameLabel->setVisible(visible);
    _spinner->setVisible(visible);
}

}
