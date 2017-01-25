#include "LoadErrorDialog.hpp"

#include <QtWidgets>

namespace Tungsten {

LoadErrorDialog::LoadErrorDialog(QWidget *parent, const JsonLoadException &e)
: QDialog(parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowSystemMenuHint)
{
    setWindowTitle("Scene load error");

    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));

    auto layout = new QVBoxLayout();

    auto descriptionText = new QLabel(e.description().c_str());
    layout->addWidget(descriptionText);

    if (e.haveExcerpt()) {
        layout->addSpacing(10);
        auto excerptText = new QLabel(e.excerpt().c_str());
        excerptText->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        excerptText->setStyleSheet("border: 1px solid #3A3939; border-radius: 5px; background-color: #333232; padding: 8px 5px;");
        layout->addWidget(excerptText);
    }

    layout->addWidget(buttonBox);

    setLayout(layout);
}

}
