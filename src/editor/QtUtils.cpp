#include "QtUtils.hpp"

#include <QtWidgets>

namespace Tungsten {

QPushButton *minimumSizeButton(const std::string &text, QWidget *parent)
{
    QPushButton *button = new QPushButton(text.c_str(), parent);
    QSize textSize = button->fontMetrics().size(Qt::TextShowMnemonic, button->text());
    QStyleOptionButton opt;
    opt.initFrom(button);
    opt.rect.setSize(textSize);
    button->setMinimumSize(button->style()->sizeFromContents(QStyle::CT_PushButton, &opt, textSize, button));
    button->setMaximumSize(button->style()->sizeFromContents(QStyle::CT_PushButton, &opt, textSize, button));

    return button;
}

void setText(QLineEdit *edit, const QString &text)
{
    edit->setText(text);
    edit->setCursorPosition(0);
}

}
