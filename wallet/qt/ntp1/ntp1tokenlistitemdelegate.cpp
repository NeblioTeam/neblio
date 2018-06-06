#include "ntp1tokenlistitemdelegate.h"

#include "guiconstants.h"
#include "ntp1/ntp1tokenlistmodel.h"

NTP1TokenListItemDelegate::NTP1TokenListItemDelegate(): QAbstractItemDelegate()
{

}

NTP1TokenListItemDelegate::~NTP1TokenListItemDelegate() {}

void NTP1TokenListItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();

    QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
    if(icon.isNull()) {
        icon = QIcon(":/images/orion");
    }

//    QIcon icon = QIcon(":/images/orion");
    QRect mainRect = option.rect;
    QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
    int xspace = DECORATION_SIZE + 8;
    int ypad = 6;
    int halfheight = (mainRect.height() - 2*ypad)/2;
    QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace, halfheight);
    QRect descriptionRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, mainRect.width() - static_cast<int>(1.1*xspace), halfheight);
    icon.paint(painter, decorationRect);

    QString amount = index.data(NTP1TokenListModel::AmountRole).toString();
    QString name = index.data(NTP1TokenListModel::TokenNameRole).toString();
    QString description = index.data(NTP1TokenListModel::TokenDescriptionRole).toString();

    QColor foreground = option.palette.color(QPalette::Text);
    bool confirmed = true;

    painter->setPen(foreground);
    painter->drawText(descriptionRect, Qt::AlignRight|Qt::AlignVCenter, description);

    if(amount < 0)
    {
        foreground = COLOR_NEGATIVE;
    }
    else if(!confirmed)
    {
        foreground = COLOR_UNCONFIRMED;
    }
    else
    {
        foreground = option.palette.color(QPalette::Text);
    }
    painter->setPen(foreground);
    QString amountText = amount + " " + name;
    if(!confirmed)
    {
        amountText = QString("[") + amountText + QString("]");
    }
    painter->drawText(amountRect, Qt::AlignLeft|Qt::AlignVCenter, amountText);

    painter->setPen(option.palette.color(QPalette::Text));
    painter->drawText(amountRect, Qt::AlignLeft|Qt::AlignVCenter, QString(""));

    painter->restore();
}

QSize NTP1TokenListItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    return QSize(DECORATION_SIZE, DECORATION_SIZE);
}
