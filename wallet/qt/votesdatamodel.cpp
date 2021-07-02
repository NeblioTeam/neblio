#include "votesdatamodel.h"

VotesDataModel::VotesDataModel(QObject* parent) : QAbstractTableModel(parent) {}

int VotesDataModel::rowCount(const QModelIndex& /*parent*/) const { return blockVotes.voteCount() + 1; }

int VotesDataModel::columnCount(const QModelIndex& /*parent*/) const { return 5; }

QVariant VotesDataModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role == Qt::TextAlignmentRole)
        return Qt::AlignCenter;

    if (role == Qt::DisplayRole) {
        if (index.row() == 0) {

            if (index.column() == 0) {
                return QVariant();
            }

            switch (index.column()) {
            case VoteDataField::ProposalID:
                return intermediaryVote.proposalID ? QVariant(*intermediaryVote.proposalID) : QVariant();
            case VoteDataField::VoteValue:
                return intermediaryVote.voteValue ? QVariant(*intermediaryVote.voteValue) : QVariant();
            case VoteDataField::StartBlock:
                return intermediaryVote.startBlock ? QVariant(*intermediaryVote.startBlock) : QVariant();
            case VoteDataField::LastBlock:
                return intermediaryVote.lastBlock ? QVariant(*intermediaryVote.lastBlock) : QVariant();
            case VoteDataField_SIZE:
                break;
            }
            return QVariant();
        }

        const boost::optional<ProposalVote> vote = blockVotes.getProposalAtIndex(index.row() - 1);
        if (!vote) {
            return QVariant();
        }

        return getEntryFieldContent(*vote, columnToField(index.column()));
    }

    return QVariant();
}

VotesDataModel::VoteDataField VotesDataModel::columnToField(int column) const
{
    if (static_cast<uint32_t>(column) < VoteDataField::VoteDataField_SIZE) {
        return static_cast<VoteDataField>(column);
    }
    return VoteDataField::VoteDataField_SIZE;
}

VotesDataModel::IntermediaryVote VotesDataModel::getIntermediaryVote() const { return intermediaryVote; }

QString VotesDataModel::getEntryFieldContent(const ProposalVote& entry, VoteDataField field) const
{
    switch (field) {
    case VoteDataField::ProposalID:
        return QVariant(entry.getProposalID()).toString();
    case VoteDataField::VoteValue:
        return QVariant(entry.getVoteValue()).toString();
    case VoteDataField::StartBlock:
        return QVariant(entry.getFirstBlockHeight()).toString();
    case VoteDataField::LastBlock:
        return QVariant(entry.getLastBlockHeight()).toString();
    case VoteDataField_SIZE:
        break;
    }
    return QString();
}

QString VotesDataModel::getFieldHeader(VoteDataField field) const
{
    switch (field) {
    case VoteDataField::ProposalID:
        return "Proposal ID";
    case VoteDataField::VoteValue:
        return "Your vote value";
    case VoteDataField::StartBlock:
        return "Vote start height";
    case VoteDataField::LastBlock:
        return "Vote last height";
    case VoteDataField_SIZE:
        break;
    }
    return QString();
}

QVariant VotesDataModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            if (section > 0) {
                return getFieldHeader(columnToField(section));
            }
        }
    }
    return QVariant();
}

void VotesDataModel::refreshAllData()
{
    beginResetModel();
    endResetModel();
}

void VotesDataModel::clearIntermediaryVote() { intermediaryVote = IntermediaryVote(); }

Qt::ItemFlags VotesDataModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return QAbstractTableModel::flags(index);
    }

    if (index.row() == 0 && index.column() != 0) {
        return (QAbstractTableModel::flags(index) | Qt::ItemFlag::ItemIsEditable) &
               ~Qt::ItemIsUserCheckable;
    }

    return QAbstractTableModel::flags(index);
}

bool VotesDataModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (index.row() == 0 && index.column() != 0 && role == Qt::EditRole) {
        bool      success = false;
        const int res     = value.toInt(&success);

        switch (index.column()) {
        case VoteDataField::ProposalID:
            intermediaryVote.proposalID = static_cast<uint32_t>(res);
            break;
        case VoteDataField::VoteValue:
            intermediaryVote.voteValue = static_cast<uint32_t>(res);
            break;
        case VoteDataField::StartBlock:
            intermediaryVote.startBlock = res;
            break;
        case VoteDataField::LastBlock:
            intermediaryVote.lastBlock = res;
            break;
        case VoteDataField_SIZE:
            break;
        }
        return success;
    }
    return true;
}
