#ifndef VECTORPROPERTY_HPP_
#define VECTORPROPERTY_HPP_

#include "Property.hpp"

#include "math/Vec.hpp"

#include <functional>

class QLineEdit;
class QLabel;

namespace Tungsten {

class ColorPickButton;
class PropertyForm;

class VectorProperty : public Property
{
    Q_OBJECT

    Vec3f _value;
    std::function<bool(Vec3f)> _setter;
    bool _isAbsorption;

    QLabel *_nameLabel;
    QLineEdit *_lineEdits[3];
    ColorPickButton *_colorPicker;

private slots:
    void changeRgb(Vec3f color);
    void changeRgb();

public:
    VectorProperty(QWidget *parent, PropertyForm &sheet, std::string name, Vec3f value,
            bool isAbsorption, std::function<bool(Vec3f)> setter);

    virtual void setVisible(bool visible) override;

public slots:
    void setValue(Vec3f color);
};

}

#endif /* VECTORPROPERTY_HPP_ */
