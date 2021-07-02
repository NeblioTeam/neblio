#ifndef VOTETABLEBUTTONDELEGATE_H
#define VOTETABLEBUTTONDELEGATE_H

#include <QObject>
#include <QStyledItemDelegate>

class VotesTableCellDelegate : public QStyledItemDelegate
{
    Q_OBJECT

    void addRemoveButtonClicked(QAbstractItemModel* model, int row);

public:
    VotesTableCellDelegate(QObject* parent = nullptr);

    void  paint(QPainter* painter, const QStyleOptionViewItem& option,
                const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    bool     editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                         const QModelIndex& index) override;
    void     setEditorData(QWidget* editor, const QModelIndex& index) const override;
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override;
    void     setModelData(QWidget* editor, QAbstractItemModel* model,
                          const QModelIndex& index) const override;
};

#endif // VOTETABLEBUTTONDELEGATE_H
