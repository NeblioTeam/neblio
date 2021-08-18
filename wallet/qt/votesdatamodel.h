#ifndef VOTESDATAMODEL_H
#define VOTESDATAMODEL_H

#include <QAbstractTableModel>
#include <QObject>

#include "globals.h"

class VotesDataModel : public QAbstractTableModel
{

public:
    // place to store a vote that's being filled
    struct IntermediaryVote
    {
        boost::optional<uint32_t> proposalID;
        boost::optional<uint32_t> voteValue;
        boost::optional<int>      startBlock;
        boost::optional<int>      lastBlock;
    };

    enum VoteDataField : uint32_t
    {
        ProposalID = 1,
        VoteValue,
        StartBlock,
        LastBlock,
        VoteDataField_SIZE
    };

private:
    Q_OBJECT

    IntermediaryVote intermediaryVote;

    QString getEntryFieldContent(const ProposalVote& entry, VoteDataField field) const;
    QString getFieldHeader(VoteDataField field) const;
    VotesDataModel::VoteDataField columnToField(int column) const;

public:
    explicit VotesDataModel(QObject* parent = nullptr);
    IntermediaryVote getIntermediaryVote() const;

    int           rowCount(const QModelIndex& parent) const override;
    int           columnCount(const QModelIndex& parent) const override;
    QVariant      data(const QModelIndex& index, int role) const override;
    QVariant      headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool          setData(const QModelIndex& index, const QVariant& value, int role) override;

    void refreshAllData();
    void clearIntermediaryVote();
};

#endif // VOTESDATAMODEL_H
