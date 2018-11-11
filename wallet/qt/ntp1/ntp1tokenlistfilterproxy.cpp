#include "ntp1tokenlistfilterproxy.h"
#include "ntp1tokenlistmodel.h"

NTP1TokenListFilterProxy::NTP1TokenListFilterProxy(QLineEdit* FilterLineEdit)
{
    filterLineEdit = FilterLineEdit;
}

bool NTP1TokenListFilterProxy::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
{
    if (filterLineEdit == NULL) {
        return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
    }

    if (filterLineEdit->text().isEmpty()) {
        return true;
    }

    QModelIndex columnsIndex = sourceModel()->index(source_row, 0, source_parent);
    if (sourceModel()
            ->data(columnsIndex, NTP1TokenListModel::AmountRole)
            .toString()
            .contains(filterLineEdit->text(), Qt::CaseInsensitive) ||
        sourceModel()
            ->data(columnsIndex, NTP1TokenListModel::TokenDescriptionRole)
            .toString()
            .contains(filterLineEdit->text(), Qt::CaseInsensitive) ||
        sourceModel()
            ->data(columnsIndex, NTP1TokenListModel::TokenIdRole)
            .toString()
            .contains(filterLineEdit->text(), Qt::CaseInsensitive) ||
        sourceModel()
            ->data(columnsIndex, NTP1TokenListModel::TokenNameRole)
            .toString()
            .contains(filterLineEdit->text(), Qt::CaseInsensitive)) {
        return true;
    } else {
        return false;
    }
}
