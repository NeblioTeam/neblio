#ifndef NEBLIOUPDATEDIALOG_H
#define NEBLIOUPDATEDIALOG_H

#include <QDialog>
#include <QTextBrowser>
#include <QGridLayout>

#include "neblioupdater.h"

class NeblioUpdateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NeblioUpdateDialog(QWidget *parent = nullptr);
    QTextBrowser* updateInfoText;
    QGridLayout*  mainLayout;

signals:

public slots:
};

#endif // NEBLIOUPDATEDIALOG_H
