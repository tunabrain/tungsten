#ifndef QTLAMBDA_HPP_
#define QTLAMBDA_HPP_

#include <functional>
#include <QObject>

namespace Tungsten {

class QtLambda: public QObject
{
    Q_OBJECT

    std::function<void()> _slot;

public:
    QtLambda(QObject *parent, std::function<void()> slot);

public slots:
    void call();
};

}

#endif /* QTLAMBDA_HPP_ */
