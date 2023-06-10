#include "MemoryWatchModel.h"

using namespace QGBA;

MemoryWatchModel::MemoryWatchModel(QObject* parent)
    : QAbstractListModel(parent) {
	m_watches.append({ 0x2033A90, "gArea", "Area" });
	m_watches.append({ 0x3003DBC, "gEntCount", "u8" });
	m_watches.append({ 0x30015A0, "gEntities", "Entity[72]" });
	m_watches.append({ 0x3003D70, "gEntityLists", "LinkedList[9]" });
	m_watches.append({ 0x30011E8, "gFirstEntities", "Entity[7]" });
	m_watches.append({ 0x2002C9C, "gGlobalFlags", "u8:1[4096]" });
	m_watches.append({ 0x2022750, "gPlayerScriptExecutionContext", "ScriptExecutionContext" });
	m_watches.append({ 0x3003F80, "gPlayerState", "PlayerState" });
	m_watches.append({ 0x3000BF0, "gRoomControls", "RoomControls" });
	m_watches.append({ 0x30010A0, "gRoomTransition", "RoomTransition" });
	m_watches.append({ 0x2034350, "gRoomVars", "RoomVars" });
	m_watches.append({ 0x2036570, "gScriptExecutionContextArray", "ScriptExecutionContext[32]" });
	m_watches.append({ 0x2002AE8, "gStats", "Stats" });
	m_watches.append({ 0x2017660, "gSmallChests", "TileEntity[8]"});
	m_watches.append({ 0x200B650, "gMapTop", "LayerStruct"});
	m_watches.append({ 0x2025EB0, "gMapBottom", "LayerStruct"});
	m_watches.append({ 0x3000F50, "gScreen", "Screen"});
}

int MemoryWatchModel::rowCount(const QModelIndex& parent) const {
	return m_watches.count();
}

QVariant MemoryWatchModel::data(const QModelIndex& index, int role) const {
	const MemoryWatch& item = m_watches[index.row()];

	switch (role) {
	case Qt::DisplayRole:
		return item.name;
	}
	return QVariant();
}

void MemoryWatchModel::addMemoryWatch(MemoryWatch watch) {
	int index = m_watches.count();
	m_watches.append(watch);
	emit dataChanged(createIndex(0, index, nullptr), createIndex(0, index, nullptr));
}

MemoryWatch MemoryWatchModel::getMemoryWatch(const QModelIndex& index) {
	return m_watches.at(index.row());
}
