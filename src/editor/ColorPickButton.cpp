#include "ColorPickButton.hpp"

#include "math/MathUtil.hpp"

#include <QColorDialog>

namespace Tungsten {

ColorPickButton::ColorPickButton(Vec3f color, QWidget *parent)
: QPushButton(parent),
  _color(color)
{
    setMinimumSize(25, 25);
    setMaximumSize(25, 25);

    changeColor(color);

    connect(this, SIGNAL(clicked()), this, SLOT(pickColor()));
}

QColor ColorPickButton::toQColor() const
{
    Vec3f ldr = _color/max(_color.max(), 1.0f);
    Vec3i rgb = clamp(Vec3i(ldr*255.0f), Vec3i(0), Vec3i(255));

    return QColor(rgb.x(), rgb.y(), rgb.z());
}

void ColorPickButton::pickColor()
{
    // Set default style sheet because QColorDialog inherits the button color otherwise
    setStyleSheet("");
    QColor color = QColorDialog::getColor(toQColor(), this, "Choose color");
    if (color.isValid()) {
        _color.x() = color.redF();
        _color.y() = color.greenF();
        _color.z() = color.blueF();

        changeColor(_color);
        emit colorChanged(_color);
    }
}

void ColorPickButton::changeColor(Vec3f color)
{
    _color = color;

    const QString Stylesheet("background-color : %1;");

    // We set the color using a style sheet to override the global style sheet
    setStyleSheet(Stylesheet.arg(toQColor().name()));
}

}
