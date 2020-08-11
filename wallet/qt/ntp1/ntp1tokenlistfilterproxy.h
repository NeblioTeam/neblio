#ifndef NTP1TOKENLISTFILTERPROXY_H
#define NTP1TOKENLISTFILTERPROXY_H

#include <QLineEdit>
#include <QSortFilterProxyModel>

class NTP1TokenListFilterProxy : public QSortFilterProxyModel
{
    QLineEdit* filterLineEdit;

public:
    NTP1TokenListFilterProxy(QLineEdit* FilterLineEdit, QObject* parent);

    // QSortFilterProxyModel interface
protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;
};

#endif // NTP1TOKENLISTFILTERPROXY_H
