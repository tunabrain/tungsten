#ifndef PATHPROPERTY_HPP_
#define PATHPROPERTY_HPP_

#include "Property.hpp"

#include <functional>

class QPushButton;
class QLineEdit;
class QLabel;

namespace Tungsten {

class PropertyForm;

class PathProperty : public Property
{
    Q_OBJECT

    std::string _value;
    std::function<bool(const std::string &)> _setter;
    std::string _searchDir;
    std::string _title;
    std::string _extensions;

    QLabel *_nameLabel;
    QPushButton *_choosePath;
    QLineEdit *_lineEdit;

private slots:
    void textEdited();
    void openPath();

public:
    PathProperty(QWidget *parent, PropertyForm &sheet, std::string name, std::string value,
            std::string searchDir, std::string title, std::string extensions,
            std::function<bool(const std::string &)> setter);

    virtual void setVisible(bool visible) override;
};

}

#endif /* PATHPROPERTY_HPP_ */
