#ifndef VERTICALSCROLLAREA_HPP_
#define VERTICALSCROLLAREA_HPP_

#include <QScrollArea>

namespace Tungsten {

class VerticalScrollArea : public QScrollArea
{
protected:
    virtual void showEvent(QShowEvent *event) override;

public:
    VerticalScrollArea(QWidget *parent);
};

}

#endif /* VERTICALSCROLLAREA_HPP_ */
