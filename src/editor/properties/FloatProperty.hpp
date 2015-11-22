#ifndef FLOATPROPERTY_HPP_
#define FLOATPROPERTY_HPP_

#include "Property.hpp"

#include <functional>

class QLineEdit;
class QLabel;

namespace Tungsten {

class PropertyForm;

class FloatProperty : public Property
{
    Q_OBJECT

    float _value;
    std::function<bool(float)> _setter;

    QLabel *_nameLabel;
    QLineEdit *_lineEdit;

private slots:
    void textEdited();

public:
    FloatProperty(QWidget *parent, PropertyForm &sheet, std::string name, float value, std::function<bool(float)> setter);

    virtual void setVisible(bool visible) override;
};

}

#endif /* FloatProperty_HPP_ */
