#include "EntityView.h"
#include "CoreController.h"

#include <QApplication>
#include <QClipboard>
#include <QNetworkReply>
#include <QtGui/QPainter>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <iostream>
#include <mgba/core/core.h>

#include "rapidjson/filereadstream.h"

#include "enums.h"
#include "warps.h"

using namespace QGBA;

EntityView::EntityView(std::shared_ptr<CoreController> controller, QWidget* parent)
    : QWidget(parent)
    , m_context(controller)
    , m_networkManager(this) {
	m_ui.setupUi(this);

	m_core = controller->thread()->core;

	FILE* fp = fopen("structs.json", "rb");
	if (!fp) {
		showError("Could not open structs.json! Make sure it is placed in the same folder.");
		return;
	}
	char readBuffer[65536];
	rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
	rapidjson::Document d;
	d.ParseStream(is);
	fclose(fp);

	// Parse definitions
	for (rapidjson::Value::ConstMemberIterator iter = d.MemberBegin(); iter != d.MemberEnd(); ++iter) {
		// printf("%s\t", iter->name.GetString());
		Definition definition = buildDefinition(iter->value);
		std::string name = iter->name.GetString();
		definitions[name] = definition;
	}

	// Automatically update contents
	connect(controller.get(), &CoreController::stopping, this, &QWidget::close);

	connect(controller.get(), &CoreController::frameAvailable, this, &EntityView::update);
	connect(controller.get(), &CoreController::paused, this, &EntityView::update);
	connect(controller.get(), &CoreController::stateLoaded, this, &EntityView::update);
	connect(controller.get(), &CoreController::rewound, this, &EntityView::update);

	//// Entity Explorer
	m_ui.treeViewEntities->setModel(&m_model);
	m_ui.treeViewEntities->expandAll();
	m_ui.treeViewEntityDetails->setModel(&m_entityDetailsModel);
	m_ui.treeViewEntityDetails->expandAll();

	connect(m_ui.treeViewEntities->selectionModel(), &QItemSelectionModel::selectionChanged, this,
	        [this](const QItemSelection& selection) {
		        if (selection.count() > 0) {
			        m_currentEntity = m_model.getEntity(selection.indexes().first());
		        } else {
			        m_currentEntity.addr = 0;
		        }
		        update();
	        });
	connect(&m_entityDetailsModel, &DetailsTreeModel::entryChanged, this, &EntityView::slotChangeEntry);
	m_ui.treeViewEntityDetails->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_ui.treeViewEntityDetails, &QWidget::customContextMenuRequested, this,
	        &EntityView::slotRightClickEntityDetails);

	m_ui.treeViewEntities->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_ui.treeViewEntities, &QWidget::customContextMenuRequested, this, &EntityView::slotRightClickEntityLists);

	// Game View
	m_ui.labelGameView->setController(controller);
	m_ui.labelGameView->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
	m_ui.labelGameView->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_ui.labelGameView, &QWidget::customContextMenuRequested, this, &EntityView::slotRightClickGameView);
	// Setup pens
	m_hitboxPen.setWidth(1);
	m_hitboxPen.setColor(Qt::red);

	m_otherHitboxPen.setWidth(1);
	m_otherHitboxPen.setColor(Qt::darkRed);

	m_circlePen.setWidth(1);
	m_circlePen.setColor(Qt::red);

	m_linePen.setWidth(2);
	m_linePen.setColor(Qt::red);

	m_smallFont.setPixelSize(7);
	m_font.setPixelSize(11);
	m_whitePen.setColor(Qt::white);

	//// Memory Viewer
	m_ui.listMemory->setModel(&m_memoryModel);
	m_ui.treeViewMemoryDetails->setModel(&m_memoryDetailsModel);
	m_ui.treeViewMemoryDetails->expandAll();

	connect(m_ui.pushButtonWatchMemory, &QPushButton::clicked, this, &EntityView::slotAddMemoryWatch);
	connect(m_ui.listMemory->selectionModel(), &QItemSelectionModel::selectionChanged, this,
	        [this](const QItemSelection& selection) {
		        m_currentWatch = m_memoryModel.getMemoryWatch(selection.indexes().first());
		        update();
	        });
	connect(&m_memoryDetailsModel, &DetailsTreeModel::entryChanged, this, &EntityView::slotChangeEntry);
	m_ui.treeViewMemoryDetails->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_ui.treeViewMemoryDetails, &QWidget::customContextMenuRequested, this,
	        &EntityView::slotRightClickMemoryDetails);
	// TODO add button to copy the "json" representation

	//// Cheats
	connect(m_ui.pushButtonFullHealth, &QPushButton::clicked, this, &EntityView::slotCheatFullHealth);
	connect(m_ui.pushButtonNearlyDead, &QPushButton::clicked, this, &EntityView::slotCheatNearlyDead);
	connect(m_ui.pushButtonAllHearts, &QPushButton::clicked, this, &EntityView::slotCheatAllHearts);
	connect(m_ui.pushButtonWarp, &QPushButton::clicked, this, &EntityView::slotCheatWarp);
	connect(m_ui.pushButtonUnsetCamera, &QPushButton::clicked, this, &EntityView::slotUnsetCamera);
	connect(m_ui.pushButtonKillAllEnemies, &QPushButton::clicked, this, &EntityView::slotCheatKillAllEnemies);
	connect(m_ui.pushButtonFullLightLevel, &QPushButton::clicked, this, &EntityView::slotCheatFullLightLevel);

	//// Scripts
	m_ui.treeViewScriptDetails->setModel(&m_scriptDetailsModel);
	m_ui.treeViewScriptDetails->expandAll();
	connect(&m_scriptDetailsModel, &DetailsTreeModel::entryChanged, this, &EntityView::slotChangeEntry);
	m_ui.treeViewScriptDetails->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_ui.treeViewScriptDetails, &QWidget::customContextMenuRequested, this,
	        &EntityView::slotRightClickScriptDetails);

	connect(m_ui.pushButtonConnectScript, &QPushButton::clicked, this, &EntityView::slotConnectScriptServer);
	m_ui.listWidgetScripts->addItem("P");
	for (int i = 0; i < 32; i++) {
		m_ui.listWidgetScripts->addItem(QString::number(i));
	}
	connect(m_ui.listWidgetScripts, &QListWidget::currentRowChanged, this, &EntityView::slotScriptContextSelected);
	m_ui.listWidgetScripts->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_ui.listWidgetScripts, &QListWidget::customContextMenuRequested, this,
	        &EntityView::slotRightClickScriptList);
}

Definition EntityView::buildDefinition(const rapidjson::Value& value) {
	Definition definition;
	auto obj = value.GetObject();
	std::string type = obj["type"].GetString();
	if (type == "struct") {
		definition.type = Type::STRUCT;
	} else if (type == "union") {
		definition.type = Type::UNION;
	} else {
		std::cerr << "Unknown type " << type << std::endl;
	}
	for (rapidjson::Value::ConstMemberIterator innerIter = obj["members"].MemberBegin();
	     innerIter != obj["members"].MemberEnd(); ++innerIter) {
		// innerIter->name.GetString();
		// printf("%s: \t", innerIter->name.GetString());
		if (innerIter->value.IsString()) {
			// printf("%s\n", innerIter->obj.GetString());
			Definition innerDefinition;
			innerDefinition.type = Type::PLAIN;
			innerDefinition.plainType = innerIter->value.GetString();
			definition.members.push_back(std::make_pair(innerIter->name.GetString(), innerDefinition));
		} else {
			Definition innerDefinition = buildDefinition(innerIter->value);
			definition.members.push_back(std::make_pair(innerIter->name.GetString(), innerDefinition));
		}
	}
	return definition;
}

void EntityView::update() {
	// Update the currently visible tab
	switch (m_ui.tabWidget->currentIndex()) {
	case 0:
		updateEntityExplorer();
		break;
	case 1:
		updateMemoryViewer();
		break;
	case 2:
		updateCheats();
		break;
	case 3:
		updateScripts();
		break;
	}

	// Always necessary to update

	// Cheats
	if (m_ui.checkBoxFreezeHealth->isChecked()) {
		slotCheatFullHealth();
	}
}

void EntityView::updateEntityLists() {
	// Read gEntityLists
	Reader reader(m_core, 0x3003D70);
	Entry entityLists = readVar(reader, "LinkedList[9]");

	uint listAddr = 0x3003d70;
	uint i = 0;
	for (auto it : entityLists.array) {
		QList<EntityData> entityList;
		uint32_t first = it.object["first"].u32;
		if (first != 0 && first != listAddr + 8 * i) {
			// There are elements in this
			uint32_t next = first;
			uint j = 0;
			do {
				uint8_t kind = m_core->rawRead8(m_core, next + 8, -1);
				uint8_t id = m_core->rawRead8(m_core, next + 9, -1);

				EntityData entityData;
				entityData.listId = i;
				entityData.entryId = j;

				QString kindStr = kinds[kind];
				QString idStr;
				switch (kind) {
				case 3:
					idStr = enemies[id];
					break;
				case 4:
					idStr = projectiles[id];
					break;
				case 6:
					idStr = objects[id];
					break;
				case 7:
					idStr = npcs[id];
					break;
				default:
					idStr = QString("%1").arg(id, 1, 16);
					break;
				}

				entityData.kind = kind;
				entityData.addr = next;
				entityData.name = QString("%1 %2").arg(kindStr, idStr);
				entityList.append(entityData);

				next = m_core->rawRead32(m_core, next + 4, -1);
				j++;
			} while (next != 0 && next != listAddr + 8 * i);
		}
		m_model.setEntities(i, entityList);
		i++;
	}
}

void EntityView::updateEntityExplorer() {
	updateEntityLists();

	// Update currently selected entity
	Entry entity;
	if (m_currentEntity.addr != 0) {
		Reader reader = Reader(m_core, m_currentEntity.addr);
		std::string type = "Entity";
		if (m_currentEntity.kind == 9) {
			type = "Manager";
		}
		entity = readVar(reader, type);
		this->m_entityDetailsModel.setEntry(type, entity);
		// m_ui.treeViewEntityDetails->expandAll();
	} else {
		// TODO only set if not already
		entity.type = EntryType::NONE;
		this->m_entityDetailsModel.setEntry("", entity);
	}

	// Fetch game image
	const color_t* buffer = m_context->drawContext();

	int m_width = 240;
	int m_height = 160;
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	m_backing = QImage(reinterpret_cast<const uchar*>(buffer), m_width, m_height, QImage::Format_RGB16);
#else
	m_backing = QImage(reinterpret_cast<const uchar*>(buffer), m_width, m_height, QImage::Format_RGB555);
#endif
#else
	m_backing = QImage(reinterpret_cast<const uchar*>(buffer), m_width, m_height, QImage::Format_ARGB32);
	m_backing = m_backing.convertToFormat(QImage::Format_RGB32);
#endif
#ifndef COLOR_5_6_5
	m_backing = m_backing.rgbSwapped();
#endif

	// TODO create painter only once?
	QPainter painter(&m_backing);

	int16_t roomScrollX = m_core->rawRead16(m_core, 0x3000bfa, -1);
	int16_t roomScrollY = m_core->rawRead16(m_core, 0x3000bfc, -1);

	if (m_ui.checkBoxAllHitboxes->isChecked()) {
		// Draw all checkboxes
		for (int listIndex = 0; listIndex < ENTITY_LISTS; listIndex++) {
			const QList<EntityData>& list = m_model.getEntities(listIndex);
			for (int i = 0; i < list.count(); i++) {
				const EntityData& data = list.at(i);
				if (data.kind != 9) {
					int hitboxAddr = m_core->rawRead32(m_core, data.addr + 0x48, -1);
					if (hitboxAddr != 0) {
						if (data.addr == m_currentEntity.addr) {
							painter.setPen(m_hitboxPen);
						} else {
							painter.setPen(m_otherHitboxPen);
						}

						Entry hitbox = readVar(hitboxAddr, "Hitbox");
						int16_t entityX = m_core->rawRead16(m_core, data.addr + 0x2E, -1);
						int16_t entityY = m_core->rawRead16(m_core, data.addr + 0x32, -1);

						// Draw hitbox
						painter.drawRect(entityX - roomScrollX + hitbox.object["offset_x"].s8 - hitbox.object["width"].u8,
						                 entityY - roomScrollY + hitbox.object["offset_y"].s8 - hitbox.object["height"].u8,
						                 hitbox.object["width"].u8 * 2, hitbox.object["height"].u8 * 2);
					}
				}
			}
		}
	} else {
		// Draw position & hitbox for the current entity
		if (m_currentEntity.addr != 0 && m_currentEntity.kind != 9 && entity.object["prev"].u32 != 0) {
			int16_t entityX = entity.object["x"].object["HALF"].object["HI"].s16;
			int16_t entityY = entity.object["y"].object["HALF"].object["HI"].s16 +
			    entity.object["height"].object["HALF"].object["HI"].s16;

			if (entity.object["hitbox"].u32 != 0) {
				Entry hitbox = readVar(entity.object["hitbox"].u32, "Hitbox");
				// Draw hitbox
				painter.setPen(m_hitboxPen);
				painter.drawRect(entityX - roomScrollX + hitbox.object["offset_x"].s8 - hitbox.object["width"].u8,
				                 entityY - roomScrollY + hitbox.object["offset_y"].s8 - hitbox.object["height"].u8,
				                 hitbox.object["width"].u8 * 2, hitbox.object["height"].u8 * 2);
			} else {
				painter.setPen(m_circlePen);
				int circleRadius = 8;
				painter.drawEllipse(entityX - roomScrollX - circleRadius, entityY - roomScrollY - circleRadius,
				                    circleRadius * 2, circleRadius * 2);
			}
			painter.setRenderHint(QPainter::Antialiasing);
			painter.setPen(m_linePen);
			painter.drawLine(10, 10, entityX - roomScrollX, entityY - roomScrollY);
		}
	}

	// Draw tiles

	int drawMap = m_ui.comboBoxMaps->currentIndex();
	if (drawMap != 0) {

		int roomOriginX = m_core->rawRead16(m_core, 0x3000bf6, -1);
		int roomOriginY = m_core->rawRead16(m_core, 0x3000bf8, -1);
		int xRelToRoom = roomScrollX - roomOriginX;
		int yRelToRoom = roomScrollY - roomOriginY;

		int tileBaseX = xRelToRoom >> 4;
		int tileBaseY = yRelToRoom >> 4;
		int offsetX = (tileBaseX << 4) - xRelToRoom;
		int offsetY = (tileBaseY << 4) - yRelToRoom;

		bool use16 = false;
		int baseAddr = 0;
		switch (drawMap) {
			case 1: // collision bottom
				baseAddr = 0x02027EB4;
				use16 = false;
				break;
			case 2: // collision top
        		baseAddr = 0x200D654;
				use16 = false;
				break;
			case 3: // mapData bottom
				baseAddr = 0x02025EB4;
				use16 = true;
				break;
			case 4: // mapData top
				baseAddr = 0x200B654;
				use16 = true;
				break;
			case 5: // mapDataClone bottom
				baseAddr = 0x02028EB4;
				use16 = true;
				break;
			case 6: // mapDataClone top
				baseAddr = 0x200E654;
				use16 = true;
				break;
		}

		if (use16) {
			painter.setFont(m_smallFont);
		} else {
			painter.setFont(m_font);
		}
		if (m_ui.checkBoxInvertTextColor->isChecked()) {
			painter.setPen(m_whitePen);
		} else {
			painter.setPen(m_blackPen);
		}

		for (int y = 0; y < (offsetY != 0 ? 11 : 10); y++) {
			for (int x = 0; x < (offsetX != 0 ? 16 : 15); x++) {
				int tile = tileBaseX + x + ((tileBaseY + y) << 6);
				uint32_t data;
				if (use16) {
					data = m_core->rawRead16(m_core, baseAddr + tile*2, -1);
				} else {
					data = m_core->rawRead8(m_core, baseAddr + tile, -1);
				}

				painter.drawText(offsetX + (x << 4), offsetY + (y << 4) + 16, QString("%1").arg(data, 1, 16));
			}
		}
	}

	painter.end();

	// TODO Does it make ta difference if we scale the resulting pixmap?
	m_backing = m_backing.scaled(m_backing.size() * m_ui.spinBoxMagnification->value());
	m_ui.labelGameView->setPixmap(QPixmap::fromImage(m_backing));
}

void EntityView::updateMemoryViewer() {
	// Update current memory watch
	if (m_currentWatch.addr != 0) {
		Entry entry;
		Reader reader = Reader(m_core, m_currentWatch.addr);
		entry = readVar(reader, m_currentWatch.type);
		this->m_memoryDetailsModel.setEntry(m_currentWatch.type, entry);
	} else {
		// TODO only set if not already
		Entry entry;
		entry.addr = 0;
		entry.type = EntryType::NONE;
		this->m_memoryDetailsModel.setEntry("", entry);
	}
}

void EntityView::updateCheats() { }

void EntityView::updateScripts() {
	// Update script lists
	int activeScripts = 0;
	for (int i = 0; i < 32; i++) {
		uint addr = 0x2036570 + i * 36;
		uint32_t scriptInstructionPointer = m_core->rawRead32(m_core, addr, -1);
		Qt::ItemFlags flags = Qt::ItemIsSelectable;
		if (scriptInstructionPointer != 0) {
			flags |= Qt::ItemIsEnabled;
			activeScripts++;
		}
		m_ui.listWidgetScripts->item(i + 1)->setFlags(flags);
		m_ui.listWidgetScripts->item(i + 1)->setSelected(m_currentScript.addr == addr);
	}

	uint addr = 0x2022750;
	uint32_t scriptInstructionPointer = m_core->rawRead32(m_core, addr, -1);
	m_ui.listWidgetScripts->item(0)->setFlags(scriptInstructionPointer == 0 ? Qt::NoItemFlags :
                                                                              Qt::ItemIsEnabled | Qt::ItemIsSelectable);
	if (scriptInstructionPointer != 0) {
		activeScripts++;
	}
	m_ui.listWidgetScripts->item(0)->setSelected(m_currentScript.addr == addr);

	m_ui.labelActiveScriptCount->setText(QString("%1 scripts active").arg(activeScripts));

	// Update current script
	if (m_currentScript.addr != 0) {
		Entry entry;
		Reader reader = Reader(m_core, m_currentScript.addr);
		entry = readVar(reader, m_currentScript.type);
		this->m_scriptDetailsModel.setEntry(m_currentScript.type, entry);
		int addr =
		    entry.object["scriptInstructionPointer"].u32; // TODO make sure the entry is a proper ScriptExecutionContext
		if (addr != m_lastScriptAddr) {
			sendScriptAddr(addr);
		}
	} else {
		// TODO only set if not already
		Entry entry;
		entry.addr = 0;
		entry.type = EntryType::NONE;
		this->m_scriptDetailsModel.setEntry("", entry);
		if (m_lastScriptAddr != 0) {
			sendScriptAddr(0);
		}
	}
}

Entry EntityView::readArray(Reader& reader, const std::string& type, uint count) {
	assert(count > 0);
	Entry result;
	result.addr = reader.m_addr;
	result.type = EntryType::ARRAY;
	for (uint i = 0; i < count; i++) {
		result.array.push_back(readVar(reader, type));
	}
	return result;
}

Entry EntityView::readStruct(Reader& reader, const Definition& definition) {
	Entry result;
	result.addr = reader.m_addr;
	result.type = EntryType::OBJECT;
	for (auto it = definition.members.begin(); it != definition.members.end(); it++) {
		switch (it->second.type) {
		case Type::PLAIN: {
			Entry entry = readVar(reader, it->second.plainType);
			result.object[it->first] = entry;
			result.objectKeys.push_back(it->first);
			break;
		}
		case Type::STRUCT: {
			Entry entry = readStruct(reader, it->second);
			result.object[it->first] = entry;
			result.objectKeys.push_back(it->first);
			break;
		}
		case Type::UNION: {
			Entry entry = readUnion(reader, it->second);
			result.object[it->first] = entry;
			result.objectKeys.push_back(it->first);
			break;
		}
		default:
			printf("UNHANDLED DEFINITION TYPE %d\n", static_cast<int>(it->second.type));
			break;
		}
	}
	return result;
}

Entry EntityView::readUnion(Reader& reader, const Definition& definition) {
	Entry result;
	result.addr = reader.m_addr;
	result.type = EntryType::OBJECT;
	// Read all union cases
	uint addr = reader.m_addr;
	for (auto it = definition.members.begin(); it != definition.members.end(); it++) {
		reader.m_addr = addr; // All union fields read from the same memory addr
		switch (it->second.type) {
		case Type::PLAIN: {
			Entry entry = readVar(reader, it->second.plainType);
			result.object[it->first] = entry;
			result.objectKeys.push_back(it->first);
			break;
		}
		case Type::STRUCT: {
			Entry entry = readStruct(reader, it->second);
			result.object[it->first] = entry;
			result.objectKeys.push_back(it->first);
			break;
		}
		case Type::UNION: {
			Entry entry = readUnion(reader, it->second);
			result.object[it->first] = entry;
			result.objectKeys.push_back(it->first);
			break;
		}
		default:
			printf("UNHANDLED DEFINITION TYPE %d\n", static_cast<int>(it->second.type));
			break;
		}
	}

	return result;
}

Entry EntityView::readBitfield(Reader& reader, uint count) {
	Entry entry;
	entry.addr = reader.m_addr; // TODO correctly handle editing for bitfields!
	// TODO handle bitfields that don't fit into an u8?
	entry.type = EntryType::U8;
	entry.u8 = reader.read_bitfield(count);
	return entry;
}

Entry EntityView::readVar(uint addr, const std::string& type) {
	Reader reader = Reader(m_core, addr);
	return readVar(reader, type);
}

Entry EntityView::readVar(Reader& reader, const std::string& type) {
	if (type.find("*") != std::string::npos) {
		Entry entry;
		entry.addr = reader.m_addr;
		entry.type = EntryType::U32;
		entry.u32 = reader.read_u32();
		return entry;
	}

	if (type.find("[") != std::string::npos) {
		std::string innerType = type.substr(0, type.find("["));
		uint count = std::atoi(type.substr(type.find("[") + 1, type.length() - 1).c_str());
		return readArray(reader, innerType, count);
	}

	if (type.find(":") != std::string::npos) {
		uint count = std::atoi(type.substr(type.find(":") + 1, type.length()).c_str());
		return readBitfield(reader, count);
	}
	if (type == "u8") {
		Entry entry;
		entry.addr = reader.m_addr;
		entry.type = EntryType::U8;
		entry.u8 = reader.read_u8();
		return entry;
	} else if (type == "s8") {
		Entry entry;
		entry.addr = reader.m_addr;
		entry.type = EntryType::S8;
		entry.s8 = reader.read_s8();
		return entry;
	} else if (type == "u16") {
		Entry entry;
		entry.addr = reader.m_addr;
		entry.type = EntryType::U16;
		entry.u16 = reader.read_u16();
		return entry;
	} else if (type == "s16") {
		Entry entry;
		entry.addr = reader.m_addr;
		entry.type = EntryType::S16;
		entry.s16 = reader.read_s16();
		return entry;
	} else if (type == "u32") {
		Entry entry;
		entry.addr = reader.m_addr;
		entry.type = EntryType::U32;
		entry.u32 = reader.read_u32();
		return entry;
	} else if (type == "s32") {
		Entry entry;
		entry.addr = reader.m_addr;
		entry.type = EntryType::S32;
		entry.s32 = reader.read_s32();
		return entry;
	}

	if (definitions.count(type) > 0) {
		if (definitions[type].type == Type::STRUCT) {
			return readStruct(reader, definitions[type]);
		} else if (definitions[type].type == Type::UNION) {
			return readUnion(reader, definitions[type]);
		}
	}
	Entry entry;
	entry.type = EntryType::ERROR;
	entry.errorMessage = "TODO: " + type + " not found";
	return entry;
}

QString EntityView::printEntry(const Entry& entry, int indentation) {
	switch (entry.type) {
	case EntryType::NONE:
		return "NONE";
	case EntryType::ERROR:
		return QString("ERROR: %1").arg(QString(entry.errorMessage.c_str()));
	case EntryType::U8:
		return QString("0x%1").arg(entry.u8, 1, 16);
	case EntryType::S8:
		return QString("0x%1").arg(entry.s8, 1, 16);
	case EntryType::U16:
		return QString("0x%1").arg(entry.u16, 1, 16);
	case EntryType::S16:
		return QString("0x%1").arg(entry.s16, 1, 16);
	case EntryType::U32:
		return QString("0x%1").arg(entry.u32, 1, 16);
	case EntryType::S32:
		return QString("0x%1").arg(entry.s32, 1, 16);
	case EntryType::OBJECT: {
		QString result = "{\n" + spaces(indentation + 2);
		for (auto it = entry.objectKeys.begin(); it != entry.objectKeys.end(); it++) {
			if (it != entry.objectKeys.begin()) {
				result += ", \n" + spaces(indentation + 2);
			}
			result += QString("%1: ").arg(it->c_str());
			const Entry& res = entry.object.at(*it);
			result += printEntry(res, indentation + 2);
		}
		return result + "\n" + spaces(indentation) + "}";
	}
	case EntryType::ARRAY: {
		QString result = "[\n" + spaces(indentation + 2);
		for (size_t i = 0; i < entry.array.size(); i++) {
			if (i != 0) {
				result += ", \n" + spaces(indentation + 2);
			}
			result += printEntry(entry.array[i], indentation + 2);
		}
		return result + "\n" + spaces(indentation) + "]";
	}
	default:
		return QString("No string conversion defined for EntryType %1").arg(static_cast<int>(entry.type));
	}
}

// TODO is there a way to do this more efficiently?
QString EntityView::spaces(int indentation) {
	QString result = "";
	for (int i = 0; i < indentation; i++) {
		result += " ";
	}
	return result;
}

uint8_t Reader::read_u8() {
	uint8_t val = m_core->rawRead8(m_core, m_addr, -1);
	m_addr += 1;
	return val;
}

uint16_t Reader::read_u16() {
	uint16_t val = m_core->rawRead16(m_core, m_addr, -1);
	m_addr += 2;
	return val;
}

uint32_t Reader::read_u32() {
	uint32_t val = m_core->rawRead32(m_core, m_addr, -1);
	m_addr += 4;
	return val;
}

uint8_t Reader::read_bitfield(uint length) {
	// return length;
	if (m_bitfieldRemaining == 0) {
		// Read the next byte
		m_bitfield = read_u8();
		m_bitfieldRemaining = 8;
	}

	if (m_bitfieldRemaining < length) {
		// Not enough bits remaining
		assert(false);
	}
	uint8_t val = m_bitfield & ((1 << (length)) - 1);
	m_bitfieldRemaining -= length;
	m_bitfield >>= length;
	// TODO TODO somehow handle that all bits of the bytes need to be taken up by the bitfield?
	return val;
}

void EntityView::slotCheatFullHealth() {
	// gStats.health = gStats.maxHealth
	uint8_t val = m_core->rawRead8(m_core, 0x2002aeb, -1);
	m_core->rawWrite8(m_core, 0x2002aea, -1, val);
}

void EntityView::slotCheatNearlyDead() {
	// gStats.health = 1
	m_core->rawWrite8(m_core, 0x2002aea, -1, 1);
}

void EntityView::slotCheatAllHearts() {
	// gStats.maxHealth = 0xa0
	m_core->rawWrite8(m_core, 0x2002aeb, -1, 0xa0);
}

void EntityView::slotCheatWarp() {
	// https://github.com/Straylite/MinishCap-Command-Box/blob/master/commands/warp.lua
	int area = m_ui.spinBoxArea->value();
	int room = m_ui.spinBoxRoom->value();
	uint8_t Awarp = area;
	uint8_t Rwarp = room;
	uint16_t xwarp = 0;
	uint16_t ywarp = 0;
	uint8_t lwarp = 1;

	std::string key = "a" + std::to_string(area) + "r" + std::to_string(room);
	if (warpPoints.count(key) > 0) {
		WarpPoint warp = warpPoints[key];
		Awarp = warp.Awarp;
		Rwarp = warp.Rwarp;
		xwarp = warp.xwarp;
		ywarp = warp.ywarp;
		lwarp = warp.lwarp;
	} else {
		std::cerr << "No warp point defined for " << area << ", " << room << std::endl;
	}

	m_core->rawWrite8(m_core, 0x03000FD2, -1, 0xF8); // White Transition
	m_core->rawWrite8(m_core, 0x030010A8, -1, 0x01); // Initializing Teleport
	m_core->rawWrite8(m_core, 0x030010AC, -1, Awarp); // Area
	m_core->rawWrite8(m_core, 0x030010AD, -1, Rwarp); // Room
	m_core->rawWrite8(m_core, 0x030010AF, -1, 0x00); // No Cucco
	m_core->rawWrite16(m_core, 0x030010B0, -1, xwarp); // Coordinates
	m_core->rawWrite16(m_core, 0x030010B2, -1, ywarp); // Coordinates
	m_core->rawWrite8(m_core, 0x030010B4, -1, lwarp); // Layer
}

void EntityView::slotAddMemoryWatch() {
	bool ok;
	MemoryWatch watch;
	watch.addr = m_ui.lineEditAddress->text().toUInt(&ok, 16);
	if (!ok) {
		std::cerr << "Invalid address: " << m_ui.lineEditAddress->text().toStdString() << std::endl;
		return;
	}
	watch.type = m_ui.lineEditType->text().toStdString();
	watch.name = m_ui.lineEditName->text() + QString(" (0x%1)").arg(watch.addr, 1, 16);
	m_memoryModel.addMemoryWatch(watch);

	m_ui.lineEditName->setText("");
	m_ui.lineEditAddress->setText("");
	m_ui.lineEditType->setText("");
}

void EntityView::slotCheatKillAllEnemies() {
	updateEntityLists();
	for (int listIndex = 0; listIndex < ENTITY_LISTS; listIndex++) {
		const QList<EntityData>& list = m_model.getEntities(listIndex);
		for (int i = 0; i < list.count(); i++) {
			const EntityData& data = list.at(i);
			if (data.kind == 3) {
				// Set currentHealth to 0
				m_core->rawWrite8(m_core, data.addr + 0x45, -1, 0);
			}
		}
	}
}

void EntityView::slotCheatFullLightLevel() {
	m_core->rawWrite16(m_core, 0x203435c, -1, 0x100);
}

void EntityView::showError(const QString& message) {
	std::cerr << message.toStdString() << std::endl;
	QMessageBox::critical(this, "Error", message);
}

void EntityView::slotChangeEntry(const Entry& entry, int value) {
	printf("Set %x to %x\n", entry.addr, value);
	switch (entry.type) {
	case EntryType::U8:
	case EntryType::S8:
		m_core->rawWrite8(m_core, entry.addr, -1, value);
		break;
	case EntryType::U16:
	case EntryType::S16:
		m_core->rawWrite16(m_core, entry.addr, -1, value);
		break;
	case EntryType::U32:
	case EntryType::S32:
		m_core->rawWrite32(m_core, entry.addr, -1, value);
		break;
	case EntryType::NONE:
	case EntryType::ERROR:
	case EntryType::OBJECT:
	case EntryType::ARRAY:
		std::cerr << "Cannot change this datatype" << std::endl;
	}
}

void EntityView::slotRightClickGameView(const QPoint& pos) {
	QMenu menu(this);
	menu.addAction("Select entity", this, &EntityView::slotGameViewSelect);
	menu.addAction("Teleport here", this, &EntityView::slotGameViewTeleport);
	m_currentGameViewClick = pos / m_ui.spinBoxMagnification->value();
	menu.exec(m_ui.labelGameView->mapToGlobal(pos));
}

void EntityView::slotGameViewTeleport() {
	int roomScrollX = m_core->rawRead16(m_core, 0x3000bfa, -1);
	int roomScrollY = m_core->rawRead16(m_core, 0x3000bfc, -1);
	int newX = roomScrollX + m_currentGameViewClick.x();
	int newY = roomScrollY + m_currentGameViewClick.y();
	m_core->rawWrite16(m_core, 0x300118e, -1, newX);
	m_core->rawWrite16(m_core, 0x3001192, -1, newY);
}

void EntityView::slotGameViewSelect() {
	int roomScrollX = m_core->rawRead16(m_core, 0x3000bfa, -1);
	int roomScrollY = m_core->rawRead16(m_core, 0x3000bfc, -1);

	// First try to select by the hitbox.
	for (int listIndex = 0; listIndex < ENTITY_LISTS; listIndex++) {
		const QList<EntityData>& list = m_model.getEntities(listIndex);
		for (int i = 0; i < list.count(); i++) {
			const EntityData& data = list.at(i);
			if (data.kind != 9) {
				int hitboxAddr = m_core->rawRead32(m_core, data.addr + 0x48, -1);
				if (hitboxAddr != 0) {
					Entry hitbox = readVar(hitboxAddr, "Hitbox");
					int16_t entityX = m_core->rawRead16(m_core, data.addr + 0x2E, -1);
					int16_t entityY = m_core->rawRead16(m_core, data.addr + 0x32, -1);

					int x = entityX - roomScrollX + hitbox.object["offset_x"].s8 - hitbox.object["width"].u8;
					int y = entityY - roomScrollY + hitbox.object["offset_y"].s8 - hitbox.object["height"].u8;
					int width = hitbox.object["width"].u8 * 2;
					int height = hitbox.object["height"].u8 * 2;
					int clickX = m_currentGameViewClick.x();
					int clickY = m_currentGameViewClick.y();
					if (clickX >= x && clickX <= x + width && clickY >= y && clickY <= y + height) {
						std::cout << "Selected " << listIndex << ":" << i << " via hitbox" << std::endl;
						m_currentEntityClick = data;
						slotSelectClickedEntity();
						return;
					}
				}
			}
		}
	}

	// No hitbox hit? Try a 10x10 box around each object.
	for (int listIndex = 0; listIndex < ENTITY_LISTS; listIndex++) {
		const QList<EntityData>& list = m_model.getEntities(listIndex);
		for (int i = 0; i < list.count(); i++) {
			const EntityData& data = list.at(i);
			if (data.kind != 9) {
				int16_t entityX = m_core->rawRead16(m_core, data.addr + 0x2E, -1);
				int16_t entityY = m_core->rawRead16(m_core, data.addr + 0x32, -1);

				const int fakeHitboxRadius = 5;

				int x = entityX - roomScrollX - fakeHitboxRadius;
				int y = entityY - roomScrollY - fakeHitboxRadius;
				int width = fakeHitboxRadius * 2;
				int height = fakeHitboxRadius * 2;
				int clickX = m_currentGameViewClick.x();
				int clickY = m_currentGameViewClick.y();
				if (clickX >= x && clickX <= x + width && clickY >= y && clickY <= y + height) {
					std::cout << "Selected " << listIndex << ":" << i << " via rect" << std::endl;
					m_currentEntityClick = data;
					slotSelectClickedEntity();
					return;
				}
			}
		}
	}

	// Did not select anything.
	std::cout << "Selected nothing" << std::endl;
	m_ui.treeViewEntities->setCurrentIndex(QModelIndex());
}

void EntityView::slotUnsetCamera() {
	// gRoomControls.cameraTarget = 0
	m_core->rawWrite32(m_core, 0x3000c20, -1, 0);
}

void EntityView::slotRightClickEntityDetails(const QPoint& pos) {
	QModelIndex index = m_ui.treeViewEntityDetails->indexAt(pos);
	if (index.isValid()) {
		m_currentDetailsClick = m_entityDetailsModel.getEntry(index);
		if (m_currentDetailsClick.type != EntryType::NONE && m_currentDetailsClick.type != EntryType::ERROR) {
			showDetailsContextMenu(m_ui.treeViewEntityDetails->viewport()->mapToGlobal(pos),
			                       m_entityDetailsModel.getKey(index));
		}
	}
}

void EntityView::slotRightClickMemoryDetails(const QPoint& pos) {
	QModelIndex index = m_ui.treeViewMemoryDetails->indexAt(pos);
	if (index.isValid()) {
		m_currentDetailsClick = m_memoryDetailsModel.getEntry(index);
		if (m_currentDetailsClick.type != EntryType::NONE && m_currentDetailsClick.type != EntryType::ERROR) {
			showDetailsContextMenu(m_ui.treeViewMemoryDetails->viewport()->mapToGlobal(pos),
			                       m_memoryDetailsModel.getKey(index));
		}
	}
}

void EntityView::slotRightClickScriptDetails(const QPoint& pos) {
	QModelIndex index = m_ui.treeViewScriptDetails->indexAt(pos);
	if (index.isValid()) {
		m_currentDetailsClick = m_scriptDetailsModel.getEntry(index);
		if (m_currentDetailsClick.type != EntryType::NONE && m_currentDetailsClick.type != EntryType::ERROR) {
			showDetailsContextMenu(m_ui.treeViewScriptDetails->viewport()->mapToGlobal(pos),
			                       m_scriptDetailsModel.getKey(index));
		}
	}
}

void EntityView::showDetailsContextMenu(const QPoint& pos, const std::string& key) {
	QMenu menu(this);
	switch (m_currentDetailsClick.type) {
	case EntryType::U8:
	case EntryType::S8:
	case EntryType::U16:
	case EntryType::S16:
	case EntryType::U32:
	case EntryType::S32:
		menu.addAction("Copy Value", this, &EntityView::slotDetailsCopyValue);
		break;
	default:
		break;
	}
	menu.addAction("Copy Address", this, &EntityView::slotDetailsCopyAddress);
	if (key == "cutsceneBeh") {
		menu.addAction("Show Script", this, &EntityView::slotShowScript);
	}
	menu.exec(pos);
}

void EntityView::slotDetailsCopyAddress() {
	QApplication::clipboard()->setText(QString("0x%1").arg(m_currentDetailsClick.addr, 1, 16));
}
void EntityView::slotDetailsCopyValue() {
	int value = 0;
	bool copy = false;
	switch (m_currentDetailsClick.type) {
	case EntryType::U8:
		value = m_currentDetailsClick.u8;
		copy = true;
		break;
	case EntryType::S8:
		value = m_currentDetailsClick.s8;
		copy = true;
		break;
	case EntryType::U16:
		value = m_currentDetailsClick.u16;
		copy = true;
		break;
	case EntryType::S16:
		value = m_currentDetailsClick.s16;
		copy = true;
		break;
	case EntryType::U32:
		value = m_currentDetailsClick.u32;
		copy = true;
		break;
	case EntryType::S32:
		value = m_currentDetailsClick.s32;
		copy = true;
		break;
	default:
		break;
	}
	if (copy) {
		QApplication::clipboard()->setText(QString("0x%1").arg(value, 1, 16));
	}
}

void EntityView::slotRightClickEntityLists(const QPoint& pos) {
	QModelIndex index = m_ui.treeViewEntities->indexAt(pos);
	if (index.isValid()) {
		m_currentEntityClick = m_model.getEntity(index);
		if (m_currentEntityClick.addr != 0) {
			QMenu menu(this);

			if (m_currentEntityClick.kind != 9) {
				menu.addAction("Set as Camera Target", this, &EntityView::slotSetAsCameraTarget);
				// Read cutsceneBeh
				uint sec = m_core->rawRead32(m_core, m_currentEntityClick.addr + 0x84, -1);
				if (sec == 0x2022750 || (sec >= 0x2036570 && sec <= 0x2036570 + 32 * 36)) {
					menu.addAction("Show Script", this, &EntityView::slotShowScriptFromEntity);
				}
			}
			menu.addAction("Delete Entity", this, &EntityView::slotDeleteEntity);

			menu.exec(m_ui.treeViewEntities->viewport()->mapToGlobal(pos));
		}
	}
}

void EntityView::slotSetAsCameraTarget() {
	// gRoomControls.cameraTarget = entity.addr
	m_core->rawWrite32(m_core, 0x3000c20, -1, m_currentEntityClick.addr);
}

void EntityView::slotDeleteEntity() {
	// TODO does not completely clear the entity. Might have side effects!
	/*ent->spriteSettings.draw = 0;
	ent->collisionFlags = 0;
	ent->contactFlags = 0;
	ent->knockbackDuration = 0;
	ent->health = 0;*/
	// UnlinkEntity(ent);
	/*if (ent == gUpdateContext.current_entity) {
        gUpdateContext.current_entity = ent->prev;
    }*/
	uint32_t prev = m_core->rawRead32(m_core, m_currentEntityClick.addr, -1);
	uint32_t next = m_core->rawRead32(m_core, m_currentEntityClick.addr + 4, -1);
    // ent->prev->next = ent->next;
	m_core->rawWrite32(m_core, prev + 4, -1, next);
    //ent->next->prev = ent->prev;
	m_core->rawWrite32(m_core, next, -1, prev);

	// ent->next = NULL;
	m_core->rawWrite32(m_core, m_currentEntityClick.addr + 4, -1, 0);
	//ent->prev = (Entity*)0xffffffff;
	m_core->rawWrite32(m_core, m_currentEntityClick.addr, -1, 0xffffffff);
}

//// Scripts

void EntityView::slotConnectScriptServer() {
	m_ui.pushButtonConnectScript->setEnabled(false);
	QNetworkReply* reply = m_networkManager.get(QNetworkRequest(QUrl("http://localhost:10244/connect")));
	m_ui.labelScriptConnectionStatus->setText("Connecting...");
	connect(reply, &QNetworkReply::finished, this, [reply, this]() {
		if (reply->error()) {
			this->m_ui.labelScriptConnectionStatus->setText("ERROR: " + reply->errorString());
			this->setConnectedToScriptServer(false);
			return;
		}

		QString answer = reply->readAll();
		if (answer == "ok") {
			this->m_ui.labelScriptConnectionStatus->setText("Connected.");
			this->setConnectedToScriptServer(true);
		} else {
			this->m_ui.labelScriptConnectionStatus->setText("ERROR: " + answer);
			this->setConnectedToScriptServer(false);
		}
	});
}

void EntityView::setConnectedToScriptServer(bool connected) {
	m_connectedToScriptServer = connected;
	if (connected) {
		m_lastScriptAddr = -1;
	}
	m_ui.pushButtonConnectScript->setEnabled(!connected);
}

void EntityView::sendScriptAddr(int addr) {
	if (m_connectedToScriptServer) {
		m_lastScriptAddr = addr;
		m_networkManager.get(QNetworkRequest(QString("http://localhost:10244/script?addr=%1").arg(addr)));
		// TODO set disconnected if any request fails
	} else if (m_lastScriptAddr != 0) {
		m_lastScriptAddr = 0;
	}
}

void EntityView::slotShowScript() {
	m_ui.tabWidget->setCurrentIndex(3); // Show scripts tab
	m_currentScript.type = "ScriptExecutionContext";
	m_currentScript.addr = m_currentDetailsClick.u32;
	m_lastScriptAddr = -1; // Force resend to server
}

void EntityView::slotShowScriptFromEntity() {
	m_ui.tabWidget->setCurrentIndex(3); // Show scripts tab
	m_currentScript.type = "ScriptExecutionContext";
	m_currentScript.addr = m_core->rawRead32(m_core, m_currentEntityClick.addr + 0x84, -1);
	m_lastScriptAddr = -1; // Force resend to server
}

void EntityView::slotScriptContextSelected(int row) {
	int addr = 0x2022750;
	if (row != 0) {
		addr = 0x2036570 + (row - 1) * 36;
	}
	m_currentScript.type = "ScriptExecutionContext";
	m_currentScript.addr = addr;
	m_lastScriptAddr = -1; // Force resend to server
	update();
}

void EntityView::slotRightClickScriptList(const QPoint& pos) {
	QModelIndex index = m_ui.listWidgetScripts->indexAt(pos);
	if (index.isValid()) {
		updateEntityLists();
		// Search the entity that is running this script
		int addr = 0x2022750;
		if (index.row() != 0) {
			addr = 0x2036570 + (index.row() - 1) * 36;
		}
		for (int listIndex = 0; listIndex < ENTITY_LISTS; listIndex++) {
			const QList<EntityData>& list = m_model.getEntities(listIndex);
			for (int i = 0; i < list.count(); i++) {
				const EntityData& data = list.at(i);
				if (data.kind != 9) {
					int cutsceneBeh = m_core->rawRead32(m_core, data.addr + 0x84, -1);
					if (cutsceneBeh == addr) {
						std::cout << "Select entity " << listIndex << ":" << i << std::endl;
						m_currentEntityClick = data;
						QMenu menu(this);
						menu.addAction("Show Corresponding Entity", this, &EntityView::slotSelectClickedEntity);
						menu.exec(m_ui.listWidgetScripts->viewport()->mapToGlobal(pos));
						return;
					}
				}
			}
		}
	}
}

void EntityView::slotSelectClickedEntity() {
	m_ui.tabWidget->setCurrentIndex(0); // Show entity explorer tab
	m_ui.treeViewEntities->setCurrentIndex(m_model.getIndex(m_currentEntityClick));
}