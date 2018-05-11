#ifndef NTP1TOKENLISTFILTERPROXY_H
#define NTP1TOKENLISTFILTERPROXY_H

#include <QSortFilterProxyModel>
#include <QLineEdit>

class NTP1TokenListFilterProxy : public QSortFilterProxyModel
{
    QLineEdit* filterLineEdit;
public:
    NTP1TokenListFilterProxy(QLineEdit *FilterLineEdit = NULL);

    // QSortFilterProxyModel interface
protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;
};

#endif // NTP1TOKENLISTFILTERPROXY_H
