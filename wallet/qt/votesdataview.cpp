#include "votesdataview.h"

#include <QAbstractItemModel>
#include <QHeaderView>

int VotesDataView::getMaxHeaderWidth() const
{
    QAbstractItemModel* model = this->model();
    if (!model) {
        return 150;
    }

    int max = 0;

    for (int i = 0; i < model->columnCount(); i++) {
        const QString text =
            model->headerData(i, Qt::Orientation::Horizontal, Qt::DisplayRole).toString();
        int val = this->horizontalHeader()->fontMetrics().horizontalAdvance(text);

        if (val > max) {
            max = val;
        }
    }
    return max;
}

int VotesDataView::getTotalHeaderTextWidth() const
{
    QAbstractItemModel* model = this->model();
    if (!model) {
        return 0;
    }

    int total = 0;

    for (int i = 0; i < model->columnCount(); i++) {
        const QString text =
            model->headerData(i, Qt::Orientation::Horizontal, Qt::DisplayRole).toString();
        total += this->horizontalHeader()->fontMetrics().horizontalAdvance(text);
    }
    return total;
}

void VotesDataView::resizeEvent(QResizeEvent* event)
{
    QTableView::resizeEvent(event);
    this->resizeColumnsToContents();
    this->horizontalHeader()->setStretchLastSection(false);
    this->horizontalHeader()->setStretchLastSection(true);
}

VotesDataView::VotesDataView(QWidget* parent) : QTableView(parent)
{
    itemDelegate = new VotesTableCellDelegate(this);
    this->setItemDelegate(itemDelegate);
    this->setWordWrap(true);
    this->horizontalHeader()->setSelectionMode(QAbstractItemView::NoSelection);
    this->setSelectionBehavior(QAbstractItemView::SelectRows);
    connect(this->horizontalHeader(), &QHeaderView::geometriesChanged, this,
            &VotesDataView::slot_geometryChanged);
    //    connect(this->horizontalHeader(), &QHeaderView::sectionResized, this,
    //            &VotesDataView::slot_headerResized);
}

void VotesDataView::setModel(QAbstractItemModel* model)
{
    QTableView::setModel(model);
    this->resizeRowsToContents();
    this->resizeColumnsToContents();
    this->horizontalHeader()->setStretchLastSection(false);
    this->horizontalHeader()->setStretchLastSection(true);
    this->resizeColumnsToContents();
    horizontalHeader()->setResizeMode(QHeaderView::Stretch);
}

void VotesDataView::slot_headerResized(int /*logicalIndex*/, int /*oldSize*/, int /*newSize*/)
{
    //    this->resizeRowsToContents();
    this->resizeColumnsToContents();
    this->horizontalHeader()->setStretchLastSection(false);
    this->horizontalHeader()->setStretchLastSection(true);
}

void VotesDataView::slot_geometryChanged()
{
    //    this->resizeRowsToContents();
    this->resizeColumnsToContents();
    this->horizontalHeader()->setStretchLastSection(false);
    this->horizontalHeader()->setStretchLastSection(true);
}
