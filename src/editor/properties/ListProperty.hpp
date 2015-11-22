#ifndef LISTPROPERTY_HPP_
#define LISTPROPERTY_HPP_

#include "Property.hpp"

#include <functional>
#include <string>
#include <vector>

class QComboBox;
class QLabel;

namespace Tungsten {

class PropertyForm;

class ListProperty : public Property
{
    Q_OBJECT

    std::vector<std::string> _list;
    int _index;
    std::string _value;
    std::function<bool(const std::string &, int)> _setter;
    QLabel *_nameLabel;
    QComboBox *_comboBox;

public:
    ListProperty(QWidget *parent, PropertyForm &sheet, std::string name, std::vector<std::string> list,
            std::string value, std::function<bool(const std::string &, int)> setter);
    ListProperty(QWidget *parent, PropertyForm &sheet, std::string name, std::vector<std::string> list,
            int index, std::function<bool(const std::string &, int)> setter);

    virtual void setVisible(bool visible) override;

public slots:
    void changeActive(int idx);
};

}


#endif /* LISTPROPERTY_HPP_ */
