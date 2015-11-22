#ifndef INTPROPERTY_HPP_
#define INTPROPERTY_HPP_

#include "Property.hpp"

#include <functional>

class QSpinBox;
class QLabel;

namespace Tungsten {

class PropertyForm;

class IntProperty : public Property
{
    Q_OBJECT

    int _value;
    std::function<bool(int)> _setter;

    QLabel *_nameLabel;
    QSpinBox *_spinner;

private slots:
    void setValue(int newValue);

public:
    IntProperty(QWidget *parent, PropertyForm &sheet, std::string name, int value, int min, int max,
            std::function<bool(int)> setter);

    virtual void setVisible(bool visible) override;
};

}

#endif /* INTPROPERTY_HPP_ */
