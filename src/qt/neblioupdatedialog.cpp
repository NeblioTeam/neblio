#include "neblioupdatedialog.h"

NeblioUpdateDialog::NeblioUpdateDialog(QWidget *parent) : QDialog(parent)
{
    mainLayout = new QGridLayout(this);
    updateInfoText = new QTextBrowser(this);

    mainLayout->addWidget(updateInfoText);
    this->setModal(true);
}
