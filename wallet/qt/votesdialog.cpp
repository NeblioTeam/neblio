#include "votesdialog.h"

VotesDialog::VotesDialog(QWidget* parent) : QDialog(parent)
{
    mainLayout = new QGridLayout;

    setLayout(mainLayout);

    votesDataView    = new VotesDataView(this);
    explanationLabel = new QLabel(this);

    explanationLabel->setText("Blocks that you stake and match the block heights below will contain a "
                              "vote from you with the given values");

    votesDataModel = new VotesDataModel(votesDataView);
    votesDataView->setModel(votesDataModel);

    mainLayout->addWidget(explanationLabel);
    mainLayout->addWidget(votesDataView);

    // 150% the width of all the text in the headers
    setMinimumWidth((votesDataView->getTotalHeaderTextWidth() * 3) / 2);
    setMinimumHeight(500);

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    setWindowTitle("Votes");
}

void VotesDialog::reloadVotes() { votesDataModel->refreshAllData(); }
