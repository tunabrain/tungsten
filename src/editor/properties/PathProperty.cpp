#include "PathProperty.hpp"
#include "PropertyForm.hpp"

#include "editor/QtUtils.hpp"

#include "io/FileUtils.hpp"

#include <QtWidgets>

namespace Tungsten {

PathProperty::PathProperty(QWidget *parent, PropertyForm &sheet, std::string name, std::string value,
        std::string searchDir, std::string title, std::string extensions,
        std::function<bool(const std::string &)> setter)
: _value(value),
  _setter(std::move(setter)),
  _searchDir(std::move(searchDir)),
  _title(std::move(title)),
  _extensions(std::move(extensions))
{

    _nameLabel = new QLabel(QString::fromStdString(name + ":"), parent);
    _lineEdit = new QLineEdit(_value.c_str(), parent);
    _lineEdit->setCursorPosition(0);
    _choosePath = minimumSizeButton("...", parent);

    QBoxLayout *horz = new QBoxLayout(QBoxLayout::LeftToRight);
    horz->setMargin(0);
    horz->addWidget(_lineEdit, 1);
    horz->addWidget(_choosePath, 0);

    sheet.addWidget(_nameLabel, sheet.rowCount(), 0);
    sheet.addLayout(horz, sheet.rowCount() - 1, 1);

    connect(_lineEdit, SIGNAL(editingFinished()), this, SLOT(textEdited()));
    connect(_choosePath, SIGNAL(clicked()), this, SLOT(openPath()));
}

void PathProperty::setVisible(bool visible)
{
    _nameLabel->setVisible(visible);
    _lineEdit->setVisible(visible);
    _choosePath->setVisible(visible);
}

void PathProperty::textEdited()
{
    std::string value = _lineEdit->text().toStdString();
    if (value != _value) {
        if (!_setter(value))
            setText(_lineEdit, QString::fromStdString(_value));
        else
            _value = value;
    }
}

void PathProperty::openPath()
{
    std::string dir =
        !_lineEdit->text().isEmpty() ? _lineEdit->text().toStdString() :
        (_searchDir.empty()) ? FileUtils::getCurrentDir().absolute().asString() :
                _searchDir;
    QString file = QFileDialog::getOpenFileName(
        nullptr,
        _title.c_str(),
        dir.c_str(),
        _extensions.c_str()
    );

    if (!file.isEmpty() && _setter(file.toStdString())) {
        _value = file.toStdString();
        setText(_lineEdit, file);
    }
}

}
