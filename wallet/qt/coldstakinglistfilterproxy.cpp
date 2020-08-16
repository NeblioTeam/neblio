#include "coldstakinglistfilterproxy.h"
#include "coldstakingmodel.h"

ColdStakingListFilterProxy::ColdStakingListFilterProxy(QLineEdit* FilterLineEdit, QObject* parent)
    : QSortFilterProxyModel(parent)
{
    filterLineEdit = FilterLineEdit;
}

bool ColdStakingListFilterProxy::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
{
    if (filterLineEdit == Q_NULLPTR) {
        return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
    }

    if (filterLineEdit->text().isEmpty()) {
        return true;
    }

    const QModelIndex columnsIndex = sourceModel()->index(source_row, 0, source_parent);
    if (sourceModel()
            ->data(columnsIndex, ColdStakingModel::ColumnIndex::OWNER_ADDRESS)
            .toString()
            .contains(filterLineEdit->text(), Qt::CaseInsensitive) ||
        sourceModel()
            ->data(columnsIndex, ColdStakingModel::ColumnIndex::OWNER_ADDRESS_LABEL)
            .toString()
            .contains(filterLineEdit->text(), Qt::CaseInsensitive) ||
        sourceModel()
            ->data(columnsIndex, ColdStakingModel::ColumnIndex::STAKING_ADDRESS)
            .toString()
            .contains(filterLineEdit->text(), Qt::CaseInsensitive) ||
        sourceModel()
            ->data(columnsIndex, ColdStakingModel::ColumnIndex::STAKING_ADDRESS_LABEL)
            .toString()
            .contains(filterLineEdit->text(), Qt::CaseInsensitive) ||
        sourceModel()
            ->data(columnsIndex, ColdStakingModel::ColumnIndex::TOTAL_STACKEABLE_AMOUNT_STR)
            .toString()
            .contains(filterLineEdit->text(), Qt::CaseInsensitive)) {
        return true;
    } else {
        return false;
    }
}
