#ifndef STRINGPROPERTY_HPP_
#define STRINGPROPERTY_HPP_

#include "Property.hpp"

#include <functional>

class QLineEdit;
class QLabel;

namespace Tungsten {

class PropertyForm;

class StringProperty : public Property
{
    Q_OBJECT

    std::string _value;
    std::function<bool(const std::string &)> _setter;
    QLabel *_nameLabel;
    QLineEdit *_lineEdit;

public:
    StringProperty(QWidget *parent, PropertyForm &sheet, std::string name, std::string value, std::function<bool(const std::string &)> setter);

    virtual void setVisible(bool visible) override;

public slots:
    void textEdited();
};

}

#endif /* BOOLPROPERTY_HPP_ */
