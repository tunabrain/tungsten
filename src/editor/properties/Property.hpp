#ifndef PROPERTY_HPP_
#define PROPERTY_HPP_

#include <QObject>

namespace Tungsten {

class Property : public QObject
{
    Q_OBJECT

public:
    Property() = default;
    virtual ~Property();

    virtual void setVisible(bool visible) = 0;
};

}

#endif /* PROPERTY_HPP_ */
