#ifndef LOADERRORDIALOG_HPP_
#define LOADERRORDIALOG_HPP_

#include "io/JsonLoadException.hpp"

#include <QDialog>

namespace Tungsten {

class LoadErrorDialog : public QDialog
{
    Q_OBJECT

public:
    LoadErrorDialog(QWidget *parent, const JsonLoadException &e);
};

}

#endif /* LOADERRORDIALOG_HPP_ */
