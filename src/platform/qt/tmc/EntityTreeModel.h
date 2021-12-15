#pragma once

#include <QtCore/QAbstractItemModel>

// TODO remove when list is no longer needed and move EntityData here
#include "EntityListModel.h"

namespace QGBA {

const int ENTITY_LISTS = 9;

class EntityTreeModel : public QAbstractItemModel {
	Q_OBJECT
public:
	EntityTreeModel(QObject* parent = nullptr);
    Qt::ItemFlags flags(const QModelIndex &index) const override;
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;

	void setEntities(int listIndex, QList<EntityData> entities);
	EntityData getEntity(const QModelIndex& index);
	const QList<EntityData>& getEntities(int listIndex);

private:
	QList<EntityData> m_entities[ENTITY_LISTS];
};

}