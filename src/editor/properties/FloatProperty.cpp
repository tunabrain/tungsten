#include "FloatProperty.hpp"
#include "PropertyForm.hpp"

#include <QtWidgets>

namespace Tungsten {

FloatProperty::FloatProperty(QWidget *parent, PropertyForm &sheet, std::string name, float value,
        std::function<bool(float)> setter)
: _value(value),
  _setter(std::move(setter))
{
    _nameLabel = new QLabel(QString::fromStdString(name + ":"), parent);
    _lineEdit = new QLineEdit(QString::number(value), parent);
    connect(_lineEdit, SIGNAL(editingFinished()), this, SLOT(textEdited()));

    sheet.addWidget(_nameLabel, sheet.rowCount(), 0);
    sheet.addWidget(_lineEdit, sheet.rowCount() - 1, 1);
}

void FloatProperty::textEdited()
{
    float newValue = _lineEdit->text().toFloat();
    if (newValue != _value) {
        if (!_setter(newValue))
            _lineEdit->setText(QString::number(_value));
        else
            _value = newValue;
    }
}

void FloatProperty::setVisible(bool visible)
{
    _nameLabel->setVisible(visible);
    _lineEdit->setVisible(visible);
}

}
