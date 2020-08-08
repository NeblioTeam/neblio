#include "coinstakinglistitemdelegate.h"

#include "coldstakingmodel.h"
#include "guiconstants.h"

CoinStakingListItemDelegate::CoinStakingListItemDelegate() : QAbstractItemDelegate() {}

CoinStakingListItemDelegate::~CoinStakingListItemDelegate() {}

void CoinStakingListItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                        const QModelIndex& index) const
{
    painter->save();

    QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
    if (icon.isNull()) {
        icon = QIcon(":/images/coldstaking");
    }

    QRect mainRect = option.rect;
    QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, ItemHeight));
    int   xspace      = DECORATION_SIZE + 8;
    int   ypad        = 6;
    int   blockheight = (mainRect.height() - 2 * ypad) / 4;
    QRect amountRect(mainRect.left() + xspace, mainRect.top() + ypad,
                     mainRect.width() - static_cast<int>(1.1 * xspace), blockheight);
    QRect whitelistRect(mainRect.left() + xspace, amountRect.y() + 3 * amountRect.height(),
                        mainRect.width() - static_cast<int>(1.1 * xspace), blockheight);

    QRect titleRect(mainRect.left() + xspace, mainRect.top() + ypad, mainRect.width() - xspace,
                    blockheight);
    QRect ownerAddrRect(mainRect.left() + xspace, titleRect.top() + titleRect.height(),
                        mainRect.width() - xspace, blockheight);
    QRect stakerAddrRect(mainRect.left() + xspace, ownerAddrRect.y() + ownerAddrRect.height(),
                         mainRect.width() - static_cast<int>(1.1 * xspace), blockheight);
    QRect addressLabelRect(mainRect.left() + xspace, stakerAddrRect.y() + stakerAddrRect.height(),
                           mainRect.width() - static_cast<int>(1.1 * xspace), blockheight);

    QString amount = index.data(ColdStakingModel::ColumnIndex::TOTAL_STACKEABLE_AMOUNT_STR).toString();
    QString owner  = index.data(ColdStakingModel::ColumnIndex::OWNER_ADDRESS).toString();
    QString ownerLabel    = index.data(ColdStakingModel::ColumnIndex::OWNER_ADDRESS_LABEL).toString();
    QString staker        = index.data(ColdStakingModel::ColumnIndex::STAKING_ADDRESS).toString();
    QString stakerLabel   = index.data(ColdStakingModel::ColumnIndex::STAKING_ADDRESS_LABEL).toString();
    bool    isWhitelisted = index.data(ColdStakingModel::ColumnIndex::IS_WHITELISTED).toBool();
    bool isRecvdDelgation = index.data(ColdStakingModel::ColumnIndex::IS_RECEIVED_DELEGATION).toBool();

    QColor foreground = option.palette.color(QPalette::Text);
    {
        QFont font = painter->font();
        font.setBold(true);
        painter->setFont(font);
        painter->setPen(foreground);
        QString titleText = "";
        if (!isRecvdDelgation) {
            titleText = "Delegated to you to stake";
            icon = QIcon(":/icons/cold_delegate_1");
        } else {
            titleText = "Delegated by you to another node";
            icon = QIcon(":/icons/cold_delegate_0");
        }
        icon.paint(painter, decorationRect);

        painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, titleText);

        font.setBold(false);
        painter->setFont(font);
    }

    foreground = option.palette.color(QPalette::Text);

    painter->setPen(foreground);
    painter->drawText(ownerAddrRect, Qt::AlignLeft | Qt::AlignVCenter, "Owner: " + owner);

    painter->setPen(option.palette.color(QPalette::Text));
    painter->drawText(amountRect, Qt::AlignLeft | Qt::AlignVCenter, QString(""));

    painter->setPen(foreground);
    painter->drawText(stakerAddrRect, Qt::AlignLeft | Qt::AlignVCenter, "Staker: " + staker);

    painter->setPen(foreground);
    QString amountText = amount + " NEBL";
    painter->drawText(amountRect, Qt::AlignRight | Qt::AlignVCenter, amountText);

    if (!isRecvdDelgation) {
        painter->drawText(whitelistRect, Qt::AlignRight | Qt::AlignVCenter,
                          isWhitelisted ? "Staking enabled" : "Staking disabled");
    }

    if (!isRecvdDelgation) {
        painter->drawText(addressLabelRect, Qt::AlignLeft | Qt::AlignVCenter,
                          "Staker address label: " +
                              (stakerLabel.isEmpty() ? "(no label)" : stakerLabel));
    } else {
        painter->drawText(addressLabelRect, Qt::AlignLeft | Qt::AlignVCenter,
                          "Owner address label: " + (ownerLabel.isEmpty() ? "(no label)" : ownerLabel));
    }

    painter->restore();
}

QSize CoinStakingListItemDelegate::sizeHint(const QStyleOptionViewItem& /*option*/,
                                            const QModelIndex& /*index*/) const
{
    return QSize(DECORATION_SIZE, ItemHeight);
}
