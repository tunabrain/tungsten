#ifndef COLORPICKBUTTON_HPP_
#define COLORPICKBUTTON_HPP_

#include "math/Vec.hpp"

#include <QPushButton>

namespace Tungsten {

class ColorPickButton : public QPushButton
{
    Q_OBJECT

    Vec3f _color;

    QColor toQColor() const;

private slots:
    void pickColor();

public:
    ColorPickButton(Vec3f color, QWidget *parent = nullptr);

public slots:
    void changeColor(Vec3f color);

signals:
    void colorChanged(Vec3f color);
};

}



#endif /* COLORPICKBUTTON_HPP_ */
