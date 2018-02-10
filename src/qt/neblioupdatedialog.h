#ifndef NEBLIOUPDATEDIALOG_H
#define NEBLIOUPDATEDIALOG_H

#include <QDialog>
#include <QTextBrowser>
#include <QGridLayout>
#include <QLabel>

#include "neblioupdater.h"

class NeblioUpdateDialog : public QDialog
{
    Q_OBJECT

    QLabel* currentVersionTitleLabel;
    QLabel* currentVersionContentLabel;
    QLabel* remoteVesionTitleLabel;
    QLabel* remoteVesionContentLabel;
    QTextBrowser* updateInfoText;
    QGridLayout*  mainLayout;
    QLabel* downloadLinkLabel;

    void setRemoteVersion(const QString& version);
    void setCurrentVersion(const QString& version);
    void setBodyText(const QString& bodyText);
    void setDownloadLink(const QString& link);

public:
    explicit NeblioUpdateDialog(QWidget *parent = 0);

    void setUpdateRelease(const NeblioReleaseInfo &rel);

signals:

public slots:
};

#endif // NEBLIOUPDATEDIALOG_H
