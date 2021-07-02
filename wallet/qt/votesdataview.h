#ifndef VOTESDATAVIEW_H
#define VOTESDATAVIEW_H

#include "votestablecelldelegate.h"
#include <QObject>
#include <QTableView>

class VotesDataView : public QTableView
{
    Q_OBJECT

    int getMaxHeaderWidth() const;

    VotesTableCellDelegate* itemDelegate;

protected:
    void resizeEvent(QResizeEvent* event) override;

public:
    VotesDataView(QWidget* parent = nullptr);
    int getTotalHeaderTextWidth() const;

    void setModel(QAbstractItemModel* model) override;
private slots:
    void slot_headerResized(int, int, int);
    void slot_geometryChanged();
};

#endif // VOTESDATAVIEW_H
