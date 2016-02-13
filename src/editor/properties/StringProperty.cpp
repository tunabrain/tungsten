#include "StringProperty.hpp"
#include "PropertyForm.hpp"

#include <QtWidgets>

namespace Tungsten {

StringProperty::StringProperty(QWidget *parent, PropertyForm &sheet, std::string name, std::string value,
        std::function<bool(const std::string &)> setter)
: _value(value),
  _setter(std::move(setter))
{
    _nameLabel = new QLabel(QString::fromStdString(name + ":"), parent);
    _lineEdit = new QLineEdit(value.c_str(), parent);
    connect(_lineEdit, SIGNAL(editingFinished()), this, SLOT(textEdited()));

    sheet.addWidget(_nameLabel, sheet.rowCount(), 0);
    sheet.addWidget(_lineEdit, sheet.rowCount() - 1, 1);
}

void StringProperty::setVisible(bool visible)
{
    _nameLabel->setVisible(visible);
    _lineEdit->setVisible(visible);
}

void StringProperty::textEdited()
{
    std::string value = _lineEdit->text().toStdString();
    if (value != _value) {
        if (!_setter(value))
            _lineEdit->setText(QString::fromStdString(_value));
        else
            _value = value;
    }
}

}
