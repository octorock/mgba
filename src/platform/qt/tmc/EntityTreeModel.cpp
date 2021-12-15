#include "EntityTreeModel.h"

#include <iostream>

using namespace QGBA;

EntityTreeModel::EntityTreeModel(QObject* parent): QAbstractItemModel(parent) {

}

Qt::ItemFlags EntityTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    return QAbstractItemModel::flags(index);
}

int EntityTreeModel::rowCount(const QModelIndex& parent) const {
    //std::cout << "rc " << parent.row() << ", " << parent.column() << std::endl;
    if (!parent.isValid()) {
        // Root count
        //std::cout << "Row count " << ENTITY_LISTS << std::endl;
        return ENTITY_LISTS;
    } else if (parent.internalPointer() == nullptr) {
        //std::cout << "Row count for " << parent.row() <<": " << m_entities[parent.row()].count()<< std::endl;
        //return 2;
        return m_entities[parent.row()].count();
    } else {
        //std::cout << "Zero rows for " << parent.row() << std::endl;
        return 0;
    }
}

int EntityTreeModel::columnCount(const QModelIndex &parent) const
{
    (void)parent;
    return 1;
}

QModelIndex EntityTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    /*if (!hasIndex(row, column, parent)) {
        std::cerr << "No index " << row << ", " << column << std::endl;
        return QModelIndex();
    }*/

    if (!parent.isValid()) {
        //std::cout << "create list index " << row << std::endl;
        return createIndex(row, column, nullptr);
    } else {
        //std::cout << "create entry index " << row << std::endl;
        return createIndex(row, column, parent.row()+1);
    }
}

QModelIndex EntityTreeModel::getIndex(const EntityData& data) {
    return createIndex(data.entryId, 0, data.listId+1);
}

QModelIndex EntityTreeModel::parent(const QModelIndex &index) const
{
    //std::cout << "get parent " << index.row() << std::endl;
    if (!index.isValid())
        return QModelIndex();

    // The list index of the parent is stored in the internalPointer
    int parent = reinterpret_cast<intptr_t>(index.internalPointer());
    //std::cout << "parent: " << parent << std::endl;
    if (parent == 0) {
        // Parent is the root
        return QModelIndex();
        //return createIndex(0, 0, nullptr);
    }
    return createIndex(parent-1, 0, nullptr);
}

QVariant EntityTreeModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || role != Qt::DisplayRole) {
        return QVariant();
    }
    //std::cout << "data " << index.row() << std::endl;
    if (index.internalPointer() == nullptr) {
        // TODO list headers?
        return QString("List %1").arg(index.row(), 1);
    }

    //return QString("Entry %1 %2").arg(index.parent().row(), 1).arg(index.row(), 1);

	const EntityData& item = m_entities[index.parent().row()][index.row()];

	switch (role) {
	case Qt::DisplayRole:
		return item.name;
	}
	return QVariant();
}

void EntityTreeModel::setEntities(int listIndex, QList<EntityData> entities) {
    //beginResetModel();
    m_entities[listIndex] = entities;
    /*m_entities.clear();
    // TODO does this break if we just replace the list?
    for (const auto& entity:entities) {
        m_entities.append(entity);
    }*/
    
    //std::cout << "data " << listIndex << ": " << entities.count() << "/" << entities.length() << std::endl;
    // TODO

    // dataChanged only works with changes of the data, but for adding and removing, we need to call layoutChanged? https://stackoverflow.com/a/41536459
    //emit dataChanged(createIndex(listIndex, 0, nullptr), createIndex(listIndex,1, nullptr));
    //emit dataChanged(createIndex(0, 0, (void*)(listIndex+1)), createIndex(entities.length()-1,0, (void*)(listIndex+1)));
    //endResetModel();
    emit layoutChanged();
}

EntityData EntityTreeModel::getEntity(const QModelIndex& index) {
    if (!index.isValid() || !index.parent().isValid()) {
        //std::cerr << "No valid parent for " << index.row() << std::endl;
        return {};
    }
    return m_entities[index.parent().row()].at(index.row());
}

const QList<EntityData>& EntityTreeModel::getEntities(int listIndex) {
    return m_entities[listIndex];
}

QVariant EntityTreeModel::headerData(int section, Qt::Orientation orientation,
                               int role) const
{
    (void) section;
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return "Entity Lists";

    return QVariant();
}