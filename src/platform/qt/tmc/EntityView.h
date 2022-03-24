#pragma once

#include "ui_EntityView.h"

#include "DetailsTreeModel.h"
#include "EntityTreeModel.h"
#include "Entry.h"
#include "MemoryWatchModel.h"
#include "rapidjson/document.h"
#include <QNetworkAccessManager>

struct mCore;

namespace QGBA {

class CoreController;

class Reader {
public:
	Reader(mCore* core, uint addr)
	    : m_addr(addr)
	    , m_core(core) { }

	uint8_t read_u8();

	int8_t read_s8() {
		int8_t val = read_u8();
		return val;
	}

	uint16_t read_u16();

	int16_t read_s16() { return read_u16(); }

	uint32_t read_u32();

	int32_t read_s32() { return read_u32(); }

	uint8_t read_bitfield(uint length);

	uint m_addr;

private:
	mCore* m_core;
	uint m_bitfield = 0;
	uint m_bitfieldRemaining = 0;
};

class EntityView : public QWidget {
	Q_OBJECT

public:
	EntityView(std::shared_ptr<CoreController> controller, QWidget* parent = nullptr);
public slots:
	void update();
	void slotCheatFullHealth();
	void slotCheatNearlyDead();
	void slotCheatAllHearts();
	void slotCheatWarp();
	void slotCheatKillAllEnemies();
	void slotCheatFullLightLevel();
	void slotAddMemoryWatch();
	void slotChangeEntry(const Entry& entry, int value);
	void slotRightClickGameView(const QPoint& pos);
	void slotGameViewTeleport();
	void slotGameViewSelect();
	void slotUnsetCamera();
	void slotRightClickEntityDetails(const QPoint& pos);
	void slotRightClickMemoryDetails(const QPoint& pos);
	void slotDetailsCopyAddress();
	void slotDetailsCopyValue();
	void slotRightClickEntityLists(const QPoint& pos);
	void slotSetAsCameraTarget();
	void slotConnectScriptServer();
	void slotShowScript();
	void slotShowScriptFromEntity();
	void slotScriptContextSelected(int row);
	void slotRightClickScriptDetails(const QPoint& pos);
	void slotRightClickScriptList(const QPoint& pos);
	void slotSelectClickedEntity();

private:
	void updateEntityLists();
	void updateEntityExplorer();
	void updateMemoryViewer();
	void updateCheats();
	void updateScripts();

	Definition buildDefinition(const rapidjson::Value& value);

	Entry readVar(uint addr, const std::string& type);
	Entry readVar(Reader& reader, const std::string& type);
	Entry readArray(Reader& reader, const std::string& type, uint count);
	Entry readStruct(Reader& reader, const Definition& definition);
	Entry readUnion(Reader& reader, const Definition& definition);
	Entry readBitfield(Reader& reader, uint count);
	QString printEntry(const Entry& entry, int indentation = 0);
	QString spaces(int indentation);
	void showError(const QString& message);
	void showDetailsContextMenu(const QPoint& pos, const std::string& key);

	void setConnectedToScriptServer(bool connected);
	void sendScriptAddr(int addr);

	Ui::EntityView m_ui;
	mCore* m_core = nullptr;
	std::map<std::string, Definition> definitions;
	EntityTreeModel m_model;
	DetailsTreeModel m_entityDetailsModel;
	EntityData m_currentEntity = { 0 };
	MemoryWatchModel m_memoryModel;
	DetailsTreeModel m_memoryDetailsModel;
	MemoryWatch m_currentWatch = { 0 };
	std::shared_ptr<CoreController> m_context = nullptr;
	QImage m_backing;

	QPen m_hitboxPen;
	QPen m_otherHitboxPen;
	QPen m_circlePen;
	QPen m_linePen;
	QFont m_smallFont;
	QFont m_font;
	QPen m_whitePen;
	QPen m_blackPen;
	QPoint m_currentGameViewClick;
	Entry m_currentDetailsClick;
	EntityData m_currentEntityClick;

	QNetworkAccessManager m_networkManager;
	bool m_connectedToScriptServer = false;
	int m_lastScriptAddr = 0;
	MemoryWatch m_currentScript = { 0 };
	DetailsTreeModel m_scriptDetailsModel;
};
}
