#include "VerticalScrollArea.hpp"

#include <QtWidgets>

namespace Tungsten {

VerticalScrollArea::VerticalScrollArea(QWidget *parent)
: QScrollArea(parent)
{
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

void VerticalScrollArea::showEvent(QShowEvent *event)
{
    setMinimumWidth(widget()->minimumSizeHint().width() + verticalScrollBar()->width());
    QScrollArea::showEvent(event);
}

}
