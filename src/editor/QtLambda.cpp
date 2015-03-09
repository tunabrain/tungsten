#include "QtLambda.hpp"

namespace Tungsten {

QtLambda::QtLambda(QObject *parent, std::function<void()> slot)
: QObject(parent),
  _slot(std::move(slot))
{
}

void QtLambda::call()
{
    _slot();
}

}
