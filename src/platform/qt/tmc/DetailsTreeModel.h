#pragma once

#include <QtCore/QAbstractItemModel>
#include "Entry.h"

namespace QGBA {


class TreeItem {

public:
    TreeItem(const QString& key, Entry entry, TreeItem* parent = nullptr);
    ~TreeItem();

    TreeItem *child(int number);
    int childCount() const;
    int columnCount() const;
    QVariant data(int column) const;
    void addChild(TreeItem* child);
    TreeItem *parent();
    bool removeChildren(int position, int count);
    int childNumber() const;
    Entry getEntry();
    void updateEntry(const Entry& entry);

    // TODO store some sort of pointer to the entity?

private:
    QVector<TreeItem*> childItems;
    Entry entry;
    TreeItem *parentItem;
    QString key;
};

class DetailsTreeModel : public QAbstractItemModel {
	Q_OBJECT
public:
	DetailsTreeModel(QObject* parent = nullptr);
    ~DetailsTreeModel();
    Qt::ItemFlags flags(const QModelIndex &index) const override;
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;

	void setEntry(const std::string& type, const Entry& entry);
	Entry getEntry(const QModelIndex& index);
signals:
    void entryChanged(const Entry& entry, int value);
private:
    TreeItem *getItem(const QModelIndex &index) const;
	Entry entry;
    std::string entryType;
    TreeItem* rootItem;
};

}