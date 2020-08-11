#ifndef COLDSTAKINGLISTFILTERPROXY_H
#define COLDSTAKINGLISTFILTERPROXY_H

#include <QLineEdit>
#include <QSortFilterProxyModel>

class ColdStakingListFilterProxy : public QSortFilterProxyModel
{
    QLineEdit* filterLineEdit;

public:
    ColdStakingListFilterProxy(QLineEdit* FilterLineEdit, QObject* parent);

    // QSortFilterProxyModel interface
protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;
};

#endif // COLDSTAKINGLISTFILTERPROXY_H
