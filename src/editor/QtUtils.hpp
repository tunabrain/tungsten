#ifndef QTUTILS_HPP_
#define QTUTILS_HPP_

#include <string>

class QPushButton;
class QLineEdit;
class QWidget;
class QString;

namespace Tungsten {

QPushButton *minimumSizeButton(const std::string &text, QWidget *parent);
void setText(QLineEdit *edit, const QString &text);

}

#endif /* QTUTILS_HPP_ */
