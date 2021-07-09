#include "votestablecelldelegate.h"

#include "votesdatamodel.h"
#include <QApplication>
#include <QMessageBox>
#include <QMouseEvent>
#include <QSpinBox>
#include <QStyleOptionButton>
#include <QStyleOptionToolButton>

void VotesTableCellDelegate::addRemoveButtonClicked(QAbstractItemModel* model, int row)
{
    if (!model) {
        QMessageBox::warning(QApplication::activeWindow(), "Internal error", "Model object is null");
        return;
    }
    VotesDataModel* vModel = dynamic_cast<VotesDataModel*>(model);
    if (!vModel) {
        QMessageBox::warning(QApplication::activeWindow(), "Internal error",
                             "Casting model object resulted in null");
        return;
    }
    if (row == 0) {
        const VotesDataModel::IntermediaryVote vd = vModel->getIntermediaryVote();
        if (vd.startBlock && vd.lastBlock && vd.proposalID && vd.voteValue) {
            const Result<ProposalVote, ProposalVoteCreationError> vote =
                ProposalVote::CreateVote(*vd.startBlock, *vd.lastBlock, *vd.proposalID, *vd.voteValue);
            if (vote.isErr()) {
                QMessageBox::warning(
                    QApplication::activeWindow(), "Failed to add vote",
                    QString::fromStdString(
                        "Error while constructing vote: " +
                        ProposalVote::ProposalVoteCreationErrorAsString(vote.UNWRAP_ERR())));
                return;
            }
            const Result<void, AddVoteError> addVoteResult = blockVotes.addVote(vote.UNWRAP());
            if (addVoteResult.isErr()) {
                QMessageBox::warning(QApplication::activeWindow(), "Failed to add vote",
                                     QString::fromStdString("Error while adding vote: " +
                                                            AllStoredVotes::AddVoteErrorAsString(
                                                                addVoteResult.UNWRAP_ERR())));
                return;
            }
            vModel->clearIntermediaryVote();
            vModel->refreshAllData();
            blockVotes.writeAllVotesAsJsonToDataDir();
        } else {
            QMessageBox::warning(QApplication::activeWindow(), "Missing field(s)",
                                 "Please fill up all the fields with valid values before attempting to "
                                 "add the vote data");
            return;
        }
    } else {
        const QVariant vStartBlock =
            model->data(model->index(row, VotesDataModel::VoteDataField::StartBlock));
        const QVariant vLastBlock =
            model->data(model->index(row, VotesDataModel::VoteDataField::LastBlock));

        const QMessageBox::StandardButton choice = QMessageBox::question(
            QApplication::activeWindow(), "Missing field(s)",
            QString("Are you sure you want to delete this vote at the block height range %1-%2")
                .arg(vStartBlock.toString(), vLastBlock.toString()),
            QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No,
            QMessageBox::StandardButton::No);
        if (choice == QMessageBox::StandardButton::Yes) {
            bool      success    = false;
            const int startBlock = vStartBlock.toInt(&success);
            if (!success) {
                QMessageBox::warning(QApplication::activeWindow(), "Internal error",
                                     "Start block conversion failed");
                return;
            }
            blockVotes.removeAllVotesAdjacentToHeight(startBlock);
            vModel->refreshAllData();
            blockVotes.writeAllVotesAsJsonToDataDir();
            return;
        }
    }
}

VotesTableCellDelegate::VotesTableCellDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void VotesTableCellDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                   const QModelIndex& index) const
{
    if (index.column() == 0) {
        QStyleOptionButton addRemoveButton;
        if (index.row() == 0) {
            addRemoveButton.icon = QIcon(":/icons/green-plus");
        } else {
            addRemoveButton.icon = QIcon(":/icons/red-minus");
        }
        addRemoveButton.iconSize = QSize(option.fontMetrics.height(), option.fontMetrics.height());
        addRemoveButton.text     = "";
        addRemoveButton.rect     = option.rect;
        QApplication::style()->drawControl(QStyle::CE_PushButtonLabel, &addRemoveButton, painter);
    } else {
        QStyledItemDelegate::paint(painter, option, index);
    }
}

QSize VotesTableCellDelegate::sizeHint(const QStyleOptionViewItem& option,
                                       const QModelIndex&          index) const
{
    if (index.column() == 0) {
        return QSize(option.fontMetrics.height(), option.fontMetrics.height());
    } else {
        return option.rect.size();
    }
}

bool VotesTableCellDelegate::editorEvent(QEvent* event, QAbstractItemModel* model,
                                         const QStyleOptionViewItem& /*option*/,
                                         const QModelIndex& index)
{
    if (!index.isValid()) {
        return false;
    }

    if (index.column() > 0) {
        // only first column needs action
        return false;
    }

    const QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);

    if (mouseEvent->button() == Qt::LeftButton && mouseEvent->type() == QEvent::MouseButtonPress) {
        addRemoveButtonClicked(model, index.row());
        event->accept();
        return true;
    }
    return false;
}

void VotesTableCellDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    QStyledItemDelegate::setEditorData(editor, index);
}

QWidget* VotesTableCellDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                              const QModelIndex& index) const
{
    if (index.column() > 0) {
        QSpinBox* editor = new QSpinBox(parent);
        editor->setFrame(false);
        editor->setMinimum(0);
        switch (index.column()) {
        case VotesDataModel::VoteDataField::ProposalID:
            editor->setMaximum(ProposalVote::MAX_PROPOSAL_ID);
            break;
        case VotesDataModel::VoteDataField::VoteValue:
            editor->setMaximum(ProposalVote::MAX_VOTE_VALUE);
            break;
        case VotesDataModel::VoteDataField::StartBlock:
            editor->setMaximum(std::numeric_limits<int>::max());
            break;
        case VotesDataModel::VoteDataField::LastBlock:
            editor->setMaximum(std::numeric_limits<int>::max());
            break;
        case VotesDataModel::VoteDataField_SIZE:
            break;
        }

        return editor;
    } else {
        return QStyledItemDelegate::createEditor(parent, option, index);
    }
}

void VotesTableCellDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                          const QModelIndex& index) const
{
    QStyledItemDelegate::setModelData(editor, model, index);
}
