#ifndef COINSTAKINGLISTITEMDELEGATE_H
#define COINSTAKINGLISTITEMDELEGATE_H

#include <QAbstractItemDelegate>
#include <QPainter>

class CoinStakingListItemDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    static const int DECORATION_SIZE = 64;
    static const int NUM_ITEMS       = 3;

    static const int ItemHeight = 1.4 * DECORATION_SIZE;

    CoinStakingListItemDelegate();

    virtual ~CoinStakingListItemDelegate();

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const Q_DECL_OVERRIDE;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const Q_DECL_OVERRIDE;
};

#endif // COINSTAKINGLISTITEMDELEGATE_H
