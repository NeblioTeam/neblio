#ifndef NTP1TOKENLISTITEMDELEGATE_H
#define NTP1TOKENLISTITEMDELEGATE_H

#include <QAbstractItemDelegate>
#include <QPainter>

class NTP1TokenListItemDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    static const int DECORATION_SIZE = 64;
    static const int NUM_ITEMS = 3;

    NTP1TokenListItemDelegate();

    virtual ~NTP1TokenListItemDelegate();

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const Q_DECL_OVERRIDE;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const Q_DECL_OVERRIDE;
};

#endif // NTP1TOKENLISTITEMDELEGATE_H
