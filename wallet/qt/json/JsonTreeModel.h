#ifndef MODELSTREEDATAMODEL_H
#define MODELSTREEDATAMODEL_H

#include <QAbstractItemModel>

#include "JsonTreeNode.h"

class JsonTreeModel : public QAbstractItemModel
{
    Q_OBJECT

    const std::shared_ptr<JsonTreeNode> DefaultRoot = std::make_shared<JsonTreeNode>();

public:
    JsonTreeModel(std::shared_ptr<JsonTreeNode> RootItem = nullptr, bool Editable = false,
                  QObject* parent = 0);
    virtual ~JsonTreeModel();

    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;

    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    //  bool setHeaderData(int section, Qt::Orientation orientation,
    //                     const QVariant &value, int role = Qt::EditRole)
    //                     override;
    //  bool insertColumns(int position, int columns,
    //                     const QModelIndex &parent = QModelIndex()) override;
    //  bool removeColumns(int position, int columns,
    //                     const QModelIndex &parent = QModelIndex()) override;
    //  bool removeRows(int position, int rows,
    //                  const QModelIndex &parent = QModelIndex()) override;

    void                          setRoot(std::shared_ptr<JsonTreeNode> RootItem);
    std::shared_ptr<JsonTreeNode> getRoot() const;
    JsonTreeNode*                 getItem(const QModelIndex& index) const;
    void                          setEditable(bool val);
    bool                          isEditable() const;

    bool addNode(int position, const std::string& key, const std::string& value,
                 json_spirit::Value_type type, const QModelIndex& parent = QModelIndex());

    bool moveNodeUp(const QModelIndex& parent = QModelIndex());
    bool moveNodeDown(const QModelIndex& parent = QModelIndex());

private:
    bool insertRows(int position, int rows, const QModelIndex& parent = QModelIndex()) override;
    bool editable = false;
    std::shared_ptr<JsonTreeNode> rootItem;
    std::shared_ptr<JsonTreeNode> toInsert;
};

#endif // MODELSTREEDATAMODEL_H
