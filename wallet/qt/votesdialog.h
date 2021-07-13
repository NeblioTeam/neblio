#ifndef VOTESDIALOG_H
#define VOTESDIALOG_H

#include "votesdatamodel.h"
#include "votesdataview.h"
#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QMenu>

class VotesDialog : public QDialog
{
    Q_OBJECT

    QGridLayout* mainLayout;

    QLabel* explanationLabel;

    VotesDataModel* votesDataModel;
    VotesDataView*  votesDataView;

    QMenu* contextMenu;

public:
    explicit VotesDialog(QWidget* parent = nullptr);
    void reloadVotes();
signals:
};

#endif // VOTESDIALOG_H
