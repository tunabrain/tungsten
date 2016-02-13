#include "VectorProperty.hpp"
#include "PropertyForm.hpp"

#include "editor/ColorPickButton.hpp"
#include "editor/QtUtils.hpp"

#include <QtWidgets>

namespace Tungsten {

VectorProperty::VectorProperty(QWidget *parent, PropertyForm &sheet, std::string name, Vec3f value,
        bool isAbsorption, std::function<bool(Vec3f)> setter)
: _value(value),
  _setter(std::move(setter)),
  _isAbsorption(isAbsorption)
{
    _nameLabel = new QLabel(QString::fromStdString(name + ":"), parent);

    QBoxLayout *horz = new QBoxLayout(QBoxLayout::LeftToRight);
    horz->setMargin(0);

    for (int i = 0; i < 3; ++i) {
        _lineEdits[i] = new QLineEdit(QString::number(value[i]), parent);
        _lineEdits[i]->setCursorPosition(0);
        connect(_lineEdits[i], SIGNAL(editingFinished()), this, SLOT(changeRgb()));
        horz->addWidget(_lineEdits[i], 1);
    }

    _colorPicker = new ColorPickButton(_isAbsorption ? std::exp(-value) : value, parent);
    connect(_colorPicker, SIGNAL(colorChanged(Vec3f)), this, SLOT(changeRgb(Vec3f)));
    horz->addWidget(_colorPicker);

    sheet.addWidget(_nameLabel, sheet.rowCount(), 0);
    sheet.addLayout(horz, sheet.rowCount() - 1, 1);
}

void VectorProperty::changeRgb(Vec3f color)
{
    if (_isAbsorption)
        color = -std::log(color);

    if (_setter(color)) {
        _value = color;

        for (int i = 0; i < 3; ++i) {
            setText(_lineEdits[i], QString::number(color[i]));
        }
    }
}

void VectorProperty::changeRgb()
{
    Vec3f color;
    for (int i = 0; i < 3; ++i)
        color[i] = _lineEdits[i]->text().toFloat();

    if (_setter(color)) {
        _value = color;
        _colorPicker->changeColor(_isAbsorption ? std::exp(-color) : color);
    }
}

void VectorProperty::setVisible(bool visible)
{
    _nameLabel->setVisible(visible);
    for (int i = 0; i < 3; ++i)
        _lineEdits[i]->setVisible(visible);
    _colorPicker->setVisible(visible);
}

void VectorProperty::setValue(Vec3f color)
{
    _value = color;
    for (int i = 0; i < 3; ++i)
        setText(_lineEdits[i], QString::number(color[i]));
    _colorPicker->changeColor(_isAbsorption ? std::exp(-color) : color);
}

}

