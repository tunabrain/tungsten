#ifndef BOOLPROPERTY_HPP_
#define BOOLPROPERTY_HPP_

#include "Property.hpp"

#include <functional>

class QCheckBox;

namespace Tungsten {

class PropertyForm;

class BoolProperty : public Property
{
    Q_OBJECT

    int _value;
    std::function<bool(bool)> _setter;
    QCheckBox *_checkbox;

public:
    BoolProperty(QWidget *parent, PropertyForm &sheet, std::string name, int value, std::function<bool(bool)> setter);

    virtual void setVisible(bool visible) override;

public slots:
    void stateChanged(int state);
};

}

#endif /* BOOLPROPERTY_HPP_ */
