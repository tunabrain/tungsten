#include "ListProperty.hpp"
#include "PropertyForm.hpp"

#include "Platform.hpp"

#include <string.h>
#include <QtWidgets>

namespace Tungsten {

ListProperty::ListProperty(QWidget *parent, PropertyForm &sheet, std::string name, std::vector<std::string> list,
        std::string value, std::function<bool(const std::string &, int)> setter)
: _list(std::move(list)),
  _index(0),
  _value(std::move(value)),
  _setter(std::move(setter))
{
    for (size_t i = 0; i < _list.size(); ++i)
        if (strcasecmp(_value.c_str(), _list[i].c_str()) == 0)
            _index = i;

    _nameLabel = new QLabel(QString::fromStdString(name + ":"), parent);
    _comboBox = new QComboBox(parent);
    for (const std::string &s : _list)
        _comboBox->addItem(s.c_str());
    _comboBox->setCurrentIndex(_index);

    connect(_comboBox, SIGNAL(activated(int)), this, SLOT(changeActive(int)));

    sheet.addWidget(_nameLabel, sheet.rowCount(), 0);
    sheet.addWidget(_comboBox, sheet.rowCount() - 1, 1);
}

ListProperty::ListProperty(QWidget *parent, PropertyForm &sheet, std::string name, std::vector<std::string> list,
        int index, std::function<bool(const std::string &, int)> setter)
: ListProperty(parent, sheet, std::move(name), list, list[index], std::move(setter))
{
}

void ListProperty::setVisible(bool visible)
{
    _nameLabel->setVisible(visible);
    _comboBox->setVisible(visible);
}

void ListProperty::changeActive(int idx)
{
    if (idx == _index)
        return;

    if (_setter(_list[idx], idx)) {
        _index = idx;
        _value = _list[idx];
    } else {
        _comboBox->setCurrentIndex(_index);
    }
}

}
