#include "EntityListModel.h"

using namespace QGBA;

EntityListModel::EntityListModel(QObject* parent)
    : QAbstractListModel(parent) { }

int EntityListModel::rowCount(const QModelIndex& parent) const {
	return m_entities.count();
}

QVariant EntityListModel::data(const QModelIndex& index, int role) const {
	const EntityData& item = m_entities[index.row()];

	switch (role) {
	case Qt::DisplayRole:
		return item.name;
	}
	return QVariant();
}

void EntityListModel::setEntities(QList<EntityData> entities) {
	// beginResetModel();
	m_entities = entities;
	/*m_entities.clear();
	// TODO does this break if we just replace the list?
	for (const auto& entity:entities) {
	    m_entities.append(entity);
	}*/
	emit dataChanged(createIndex(0, 0, nullptr), createIndex(0, entities.length() - 1, nullptr));
	// endResetModel();
}

EntityData EntityListModel::getEntity(const QModelIndex& index) {
	return m_entities.at(index.row());
}
