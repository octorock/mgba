#include "DetailsTreeModel.h"

#include <iostream>

using namespace QGBA;

TreeItem::TreeItem(const QString& key, Entry entry, TreeItem* parent)
    : entry(entry)
    , parentItem(parent)
    , key(key) {
	if (entry.type == EntryType::OBJECT) {
		for (auto& key : entry.objectKeys) {
			// const Entry* innerEntry = &entry.object.at(key);
			TreeItem* item = new TreeItem(QString(key.c_str()), entry.object.at(key), this);
			this->addChild(item);
		}
	} else if (entry.type == EntryType::ARRAY) {
		for (size_t i = 0; i < entry.array.size(); i++) {
			TreeItem* item = new TreeItem(QString("%1").arg(i, 1), entry.array[i], this);
			this->addChild(item);
		}
	} else if (parent == nullptr && entry.type != EntryType::NONE) {
		// TODO need to add this as a child so it is editable
		TreeItem* item = new TreeItem(key, entry, this);
		this->addChild(item);
	}
}

TreeItem::~TreeItem() {
	/*parentItem = nullptr;
	for (const auto& child: childItems) {
	    child->parentItem = nullptr;
	}*/
	qDeleteAll(childItems);
}

TreeItem* TreeItem::parent() {
	return parentItem;
}

TreeItem* TreeItem::child(int number) {
	if (number < 0 || number >= childItems.size())
		return nullptr;
	return childItems.at(number);
}

int TreeItem::childCount() const {
	return childItems.count();
}

int TreeItem::childNumber() const {
	if (parentItem) {
		return parentItem->childItems.indexOf(const_cast<TreeItem*>(this));
	}
	return 0;
}

int TreeItem::columnCount() const {
	switch (entry.type) {
	case EntryType::NONE:
		return 0;
	case EntryType::ARRAY:
	case EntryType::OBJECT:
		return 1;
	case EntryType::ERROR:
	case EntryType::U8:
	case EntryType::S8:
	case EntryType::U16:
	case EntryType::S16:
	case EntryType::U32:
	case EntryType::S32:
		return 2;
	}
	return 2;
	// return itemData.count();
}

QVariant TreeItem::data(int column) const {
	if (column < 0 || column > 1) {
		return QVariant();
	}
	if (column == 0) {
		return key;
	}
	switch (entry.type) {
	case EntryType::NONE:
		return "NONE";
	case EntryType::ERROR:
		return QString("ERROR: %1").arg(QString(entry.errorMessage.c_str()));
	case EntryType::U8:
		return QString("0x%1").arg(entry.u8, 1, 16);
	case EntryType::S8:
		if (entry.s8 < 0) {
			return QString("-0x%1").arg(-entry.s8, 1, 16);
		} else {
			return QString("0x%1").arg(entry.s8, 1, 16);
		}
	case EntryType::U16:
		return QString("0x%1").arg(entry.u16, 1, 16);
	case EntryType::S16:
		if (entry.s16 < 0) {
			return QString("-0x%1").arg(-entry.s16, 1, 16);
		} else {
			return QString("0x%1").arg(entry.s16, 1, 16);
		}
	case EntryType::U32:
		return QString("0x%1").arg(entry.u32, 1, 16);
	case EntryType::S32:
		if (entry.s32 < 0) {
			return QString("-0x%1").arg(-entry.s32, 1, 16);
		} else {
			return QString("0x%1").arg(entry.s32, 1, 16);
		}
	case EntryType::ARRAY:
	case EntryType::OBJECT:
		return "";
	}
	return "";
	/*	if (column < 0 || column >= itemData.size())
	        return QVariant();
	    return itemData.at(column);*/
}

void TreeItem::addChild(TreeItem* item) {
	childItems.append(item);
}

bool TreeItem::removeChildren(int position, int count) {
	if (position < 0 || position + count > childItems.size())
		return false;

	for (int row = 0; row < count; ++row)
		delete childItems.takeAt(position);

	return true;
}

Entry TreeItem::getEntry() {
	return entry;
}

void TreeItem::updateEntry(const Entry& entry) {
	this->entry = entry;
	if (entry.type == EntryType::OBJECT) {
		int i = 0;
		for (auto& key : entry.objectKeys) {
			child(i)->updateEntry(entry.object.at(key));
			i++;
		}
	} else if (entry.type == EntryType::ARRAY) {
		for (size_t i = 0; i < entry.array.size(); i++) {
			child(i)->updateEntry(entry.array[i]);
		}
	} else if (parentItem == nullptr && entry.type != EntryType::NONE) {
		// The only child is the one that is editable.
		child(0)->updateEntry(entry);
	}
}

const QString& TreeItem::getKey() {
	return key;
}

DetailsTreeModel::DetailsTreeModel(QObject* parent)
    : QAbstractItemModel(parent) {
	entry.type = EntryType::NONE;
	rootItem = new TreeItem("", entry);
}

DetailsTreeModel::~DetailsTreeModel() {
	delete rootItem;
}

TreeItem* DetailsTreeModel::getItem(const QModelIndex& index) const {
	if (index.isValid()) {
		TreeItem* item = static_cast<TreeItem*>(index.internalPointer());
		if (item)
			return item;
	}
	return rootItem;
}

int DetailsTreeModel::rowCount(const QModelIndex& parent) const {
	if (parent.isValid() && parent.column() > 0) {
		return 0;
	}

	const TreeItem* parentItem = getItem(parent);

	return parentItem ? parentItem->childCount() : 0;
	/*// std::cout << "rc " << parent.row() << ", " << parent.column() << std::endl;
	if (!parent.isValid()) {
	    // Root count
	    // std::cout << "Row count " << ENTITY_LISTS << std::endl;
	    return ENTITY_LISTS;
	} else if (parent.internalPointer() == nullptr) {
	    // std::cout << "Row count for " << parent.row() <<": " << m_entities[parent.row()].count()<< std::endl;
	    // return 2;
	    return m_entities[parent.row()].count();
	} else {
	    // std::cout << "Zero rows for " << parent.row() << std::endl;
	    return 0;
	}*/
}

int DetailsTreeModel::columnCount(const QModelIndex& parent) const {
	(void) parent;
	return 2;
	/*TreeItem* parentItem = getItem(parent);
	if (!parentItem)
	    return rootItem->columnCount();
	return parentItem->columnCount();*/
}

Qt::ItemFlags DetailsTreeModel::flags(const QModelIndex& index) const {
	if (!index.isValid())
		return Qt::NoItemFlags;

	if (index.column() != 1) {
		return QAbstractItemModel::flags(index);
	}
	TreeItem* item = getItem(index);
	switch (item->getEntry().type) {
	case EntryType::U8:
	case EntryType::S8:
	case EntryType::U16:
	case EntryType::S16:
	case EntryType::U32:
	case EntryType::S32:
		return Qt::ItemIsEditable | QAbstractItemModel::flags(index);
	case EntryType::NONE:
	case EntryType::ERROR:
	case EntryType::OBJECT:
	case EntryType::ARRAY:
		return QAbstractItemModel::flags(index);
	}
	return QAbstractItemModel::flags(index);
}

QModelIndex DetailsTreeModel::index(int row, int column, const QModelIndex& parent) const {
	if (parent.isValid() && parent.column() != 0)
		return QModelIndex();

	TreeItem* parentItem = getItem(parent);
	if (!parentItem)
		return QModelIndex();

	TreeItem* childItem = parentItem->child(row);
	if (childItem)
		return createIndex(row, column, childItem);
	return QModelIndex();
	/*	if (!parent.isValid()) {
	        // std::cout << "create list index " << row << std::endl;
	        return createIndex(row, column, nullptr);
	    } else {
	        // std::cout << "create entry index " << row << std::endl;
	        return createIndex(row, column, parent.row() + 1);
	    }*/
}

QModelIndex DetailsTreeModel::parent(const QModelIndex& index) const {
	if (!index.isValid())
		return QModelIndex();

	TreeItem* childItem = getItem(index);
	TreeItem* parentItem = childItem ? childItem->parent() : nullptr;

	if (parentItem == rootItem || !parentItem)
		return QModelIndex();

	return createIndex(parentItem->childNumber(), 0, parentItem);
	/*// std::cout << "get parent " << index.row() << std::endl;
	if (!index.isValid())
	    return QModelIndex();

	// The list index of the parent is stored in the internalPointer
	int parent = reinterpret_cast<intptr_t>(index.internalPointer());
	// std::cout << "parent: " << parent << std::endl;
	if (parent == 0) {
	    // Parent is the root
	    return QModelIndex();
	    // return createIndex(0, 0, nullptr);
	}
	return createIndex(parent - 1, 0, nullptr);*/
}

QVariant DetailsTreeModel::data(const QModelIndex& index, int role) const {
	if (!index.isValid())
		return QVariant();

	if (role != Qt::DisplayRole && role != Qt::EditRole)
		return QVariant();

	TreeItem* item = getItem(index);

	return item->data(index.column());
	/*if (!index.isValid() || role != Qt::DisplayRole) {
	    return QVariant();
	}
	// std::cout << "data " << index.row() << std::endl;
	if (index.internalPointer() == nullptr) {
	    // TODO list headers?
	    return QString("List %1").arg(index.row(), 1);
	}

	// return QString("Entry %1 %2").arg(index.parent().row(), 1).arg(index.row(), 1);

	const EntityData& item = m_entities[index.parent().row()][index.row()];

	switch (role) {
	case Qt::DisplayRole:
	    return item.name;
	}
	return QVariant();*/
}

void DetailsTreeModel::setEntry(const std::string& type, const Entry& entry) {
	if (type != this->entryType) {
		beginResetModel();
		// std::cout << "Rebuild tree" << std::endl;
		// Need to rebuild the tree

		delete rootItem;
		rootItem = new TreeItem(QString(type.c_str()), entry, nullptr);
		// if (entry.type == EntryType::OBJECT) {
		//     rootItem->removeChildren(0, rootItem->childCount()); // clear
		//     for ( auto& key : entry.objectKeys) {
		//         //const Entry* innerEntry = &entry.object.at(key);
		//         TreeItem* item = new TreeItem(QString(key.c_str()), entry.object.at(key), rootItem);
		//         rootItem->addChild(item);
		//     }
		// }
		this->entry = entry;
		endResetModel();
	} else {
		// TODO replace entries in the tree
		rootItem->updateEntry(entry);

		// TODO maybe data changed is enough?
		emit layoutChanged();
	}

	this->entryType = type;

	/*m_entities.clear();
	// TODO does this break if we just replace the list?
	for (const auto& entity:entities) {
	    m_entities.append(entity);
	}*/

	// std::cout << "data " << listIndex << ": " << entities.count() << "/" << entities.length() << std::endl;
	// TODO

	// dataChanged only works with changes of the data, but for adding and removing, we need to call layoutChanged?
	// https://stackoverflow.com/a/41536459
	// emit dataChanged(createIndex(listIndex, 0, nullptr), createIndex(listIndex,1, nullptr));
	// emit dataChanged(createIndex(0, 0, (void*)(listIndex+1)), createIndex(entities.length()-1,0,
	// (void*)(listIndex+1))); endResetModel();
}

Entry DetailsTreeModel::getEntry(const QModelIndex& index) {
	// if (!index.parent().isValid()) {
	// 	// std::cerr << "No valid parent for " << index.row() << std::endl;
	// 	return {};
	// }
	// return m_entities[index.parent().row()].at(index.row());
	TreeItem* item = getItem(index);

	if (item) {
		return item->getEntry();
	} else {
		Entry entry;
		entry.type = EntryType::ERROR;
		entry.errorMessage = "Could not fetch entry";
		return entry;
	}
}

 std::string DetailsTreeModel::getKey(const QModelIndex& index) {
	TreeItem* item = getItem(index);
	if (item) {
		return item->getKey().toStdString();
	} else {
		return "";
	}
}

QVariant DetailsTreeModel::headerData(int section, Qt::Orientation orientation, int role) const {
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
		if (rootItem) {
			switch (section) {
			case 0: {
				const Entry& entry = rootItem->getEntry();
				if (entry.type != EntryType::NONE) {
					return rootItem->data(0).toString() + QString(" 0x%1").arg(entry.addr, 1, 16);
				}
			} break;
			case 1: {
				const Entry& entry = rootItem->getEntry();
				switch (entry.type) {
				case EntryType::NONE:
					return "";
				case EntryType::ERROR:
					return QString("ERROR: %1").arg(QString(entry.errorMessage.c_str()));
				case EntryType::U8:
				case EntryType::S8:
				case EntryType::U16:
				case EntryType::S16:
				case EntryType::U32:
				case EntryType::S32:
				case EntryType::ARRAY:
				case EntryType::OBJECT:
					return "Value";
				}
            }
			}
		}
	}

	return QVariant();
}

bool DetailsTreeModel::setData(const QModelIndex& index, const QVariant& value, int role) {
	if (role != Qt::EditRole)
		return false;

	TreeItem* item = getItem(index);
	bool ok;
	int base = 10;
	if (value.toString().startsWith("0x") || value.toString().startsWith("-0x")) {
		base = 16;
	}

	int intValue = value.toString().toInt(&ok, base);
	if (ok) {
		const Entry& entry = item->getEntry();
		emit entryChanged(entry, intValue);
	}
	return ok;
	/*
	//bool result = item->setData(index.column(), value);

	if (result)
	    emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});

	return result;*/
}