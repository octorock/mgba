#include "EntityView.h"
#include "CoreController.h"

#include <mgba/core/core.h>
#include <iostream>


#include "rapidjson/filereadstream.h"

#include "enums.h"

using namespace QGBA;

EntityView::EntityView(std::shared_ptr<CoreController> controller, QWidget* parent): QWidget(parent) {
    m_ui.setupUi(this);
    m_ui.entityLists->setModel(&m_model);
    m_ui.listMemory->setModel(&m_memoryModel);

    m_core = controller->thread()->core;

    // Automatically update contents
	connect(controller.get(), &CoreController::stopping, this, &QWidget::close);

	connect(controller.get(), &CoreController::frameAvailable, this, &EntityView::update);
	connect(controller.get(), &CoreController::paused, this, &EntityView::update);
	connect(controller.get(), &CoreController::stateLoaded, this, &EntityView::update);
	connect(controller.get(), &CoreController::rewound, this, &EntityView::update);

	connect(m_ui.entityLists, &QAbstractItemView::clicked, this, [this](const QModelIndex& index) {
        m_currentEntity = m_model.getEntity(index);
	});

    FILE* fp = fopen("../../src/platform/qt/tmc/structs.json", "rb");
    if (!fp) {
        std::cerr << "COULD NOT OPEN structs.json! Please fix path." << std::endl;
        m_ui.entityInfo->setText("COULD NOT OPEN structs.json! Please fix path.");
        return;
    }
    char readBuffer[65536];
    rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
    rapidjson::Document d;
    d.ParseStream(is);
    fclose(fp);

    // Parse definitions
    for (rapidjson::Value::ConstMemberIterator iter = d.MemberBegin(); iter != d.MemberEnd(); ++iter){
        //printf("%s\t", iter->name.GetString());
        Definition definition = buildDefinition(iter->value);
        std::string name = iter->name.GetString();
        definitions[name] = definition;
    }

    //// Memory Viewer
    connect(m_ui.pushButtonWatchMemory, &QPushButton::clicked, this, &EntityView::slotAddMemoryWatch);
	connect(m_ui.listMemory, &QAbstractItemView::clicked, this, [this](const QModelIndex& index) {
        m_currentWatch = m_memoryModel.getMemoryWatch(index);
	});

    //// Cheats
    connect(m_ui.pushButtonFullHealth, &QPushButton::clicked, this, &EntityView::slotCheatFullHealth);
    connect(m_ui.pushButtonNearlyDead, &QPushButton::clicked, this, &EntityView::slotCheatNearlyDead);
    connect(m_ui.pushButtonAllHearts, &QPushButton::clicked, this, &EntityView::slotCheatAllHearts);
    connect(m_ui.pushButtonTeleport, &QPushButton::clicked, this, &EntityView::slotCheatTeleport);
}

Definition EntityView::buildDefinition(const rapidjson::Value& value) {
    Definition definition;
    auto obj = value.GetObject();
    std::string type = obj["type"].GetString();
    if (type == "struct") {
        definition.type = STRUCT;
    } else if (type == "union") {
        definition.type = UNION;
    } else {
        std::cerr << "Unknown type " << type << std::endl;
    }
    for (rapidjson::Value::ConstMemberIterator innerIter = obj["members"].MemberBegin(); innerIter != obj["members"].MemberEnd(); ++innerIter){
        //innerIter->name.GetString();
        //printf("%s: \t", innerIter->name.GetString());
        if (innerIter->value.IsString()) {
            //printf("%s\n", innerIter->obj.GetString());
            Definition innerDefinition;
            innerDefinition.type = PLAIN;
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
    /*uint16_t b = m_core->rawRead16(m_core, 0x03000000, -1);
    printf("byte 0: %x\n", b);
    b = m_core->rawRead16(m_core, 0x03000001, -1);
    printf("byte 1: %x\n", b);
    b = m_core->rawRead16(m_core, 0x03000002, -1);
    printf("byte 2: %x\n\n", b);*/

    Entry entityLists = printVar(0x3003D70, "gEntityLists", "LinkedList[9]");

    QList<EntityData> entityList;

    uint listAddr = 0x3003d70;
    uint i = 0;
    for(auto it : entityLists.array) {
        //std::vector<Entry> entities;
        uint32_t first = it.object["first"].u32;
        if (first != 0 && first != listAddr + 8 * i) {
            // There are elements in this
            uint32_t next = first;
            uint j = 0;
            do {
                uint8_t kind = m_core->rawRead8(m_core, next+8, -1);
                uint8_t id = m_core->rawRead8(m_core, next+9, -1);
                
                /*Entry entity;
                Reader reader = Reader(m_core, next);
                if (kind == 9) {
                    entity = readVar(reader, "Manager");
                } else {
                    entity = readVar(reader, "Entity");
                }
                //printEntry(entity);
                entities.push_back(entity);*/
                EntityData entityData;
                entityData.listId = i;
                entityData.entryId = j;
                // uint8_t id = entity.object["id"].u8;

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
                // idStr += " " + QString::number(id);
                // idStr += "/" + QString::number(entity.object["id"].u8);

                entityData.kind = kind;
                entityData.addr = next;
                entityData.name = QString("[%1:%2] %3 %4").arg(QString::number(i), QString::number(j), kindStr, idStr);
                entityList.append(entityData);

                next = m_core->rawRead32(m_core, next+4, -1);
                j++;
                /*printf("prev: 0x%x\n", next);
                printf("next: 0x%x\n", prev);
                printf("id: 0x%x\n", entity.object["id"].u8);*/
                //std::cout << "LOADING..."<<i<<","<<j << " " << next << std::endl;
            } while (next != 0 && next != listAddr + 8*i);
        }
        i++;
    }
    //std::cout << "Setting" << std::endl;
    m_model.setEntities(entityList);


    // Update currently selected entity
    //std::cout << "current: " << m_currentEntity.addr << std::endl;
    if (m_currentEntity.addr != 0) {
        Entry entity;
        Reader reader = Reader(m_core, m_currentEntity.addr);
        if (m_currentEntity.kind == 9) {
            entity = readVar(reader, "Manager");
        } else {
            entity = readVar(reader, "Entity");
        }
        QString text = printEntry(entity);
        this->m_ui.entityInfo->setText(text);
    }

    // Update current memory watch
    if (m_currentWatch.addr != 0) {
        Entry entity;
        Reader reader = Reader(m_core, m_currentWatch.addr);
        entity = readVar(reader, m_currentWatch.type);
        QString text = printEntry(entity);
        this->m_ui.labelMemory->setText(text);
    }
}

Entry EntityView::readArray(Reader& reader, const std::string& type, uint count) {
    assert(count > 0);
    Entry result;
    result.type = ARRAY;
    for (uint i = 0; i < count; i++) {
        result.array.push_back(readVar(reader, type));
    }
    return result;
}

Entry EntityView::readStruct(Reader& reader, const Definition& definition) {
    Entry result;
    result.type = OBJECT;
    for (auto it = definition.members.begin(); it != definition.members.end(); it++) {
        switch (it->second.type) {
            case PLAIN:
            {
                Entry entry = readVar(reader, it->second.plainType);
                result.object[it->first] = entry;
                result.objectKeys.push_back(it->first);
                break;
            }
            case STRUCT:
            {
                Entry entry = readStruct(reader, it->second);
                result.object[it->first] = entry;
                result.objectKeys.push_back(it->first);
                break;
            }
            case UNION:
            {
                Entry entry = readUnion(reader, it->second);
                result.object[it->first] = entry;
                result.objectKeys.push_back(it->first);
                break;
            }
            default:
                printf("UNHANDLED DEFINITION TYPE %d\n", it->second.type);
                break;
        }
    }
    return result;
}

Entry EntityView::readUnion(Reader& reader, const Definition& definition) {
    Entry result;
    result.type = OBJECT;
    /*// Just read the first available type
    auto it = definition.members.begin();
    switch (it->second.type) {
        case PLAIN:
        {
            Entry entry = readVar(reader, it->second.plainType);
            result.object[it->first] = entry;
            result.objectKeys.push_back(it->first);
            break;
        }
        case STRUCT:
        {
            Entry entry = readStruct(reader, it->second);
            result.object[it->first] = entry;
            result.objectKeys.push_back(it->first);
            break;
        }
        case UNION:
        {
            Entry entry = readUnion(reader, it->second);
            result.object[it->first] = entry;
            result.objectKeys.push_back(it->first);
            break;
        }
        default:
            printf("UNHANDLED DEFINITION TYPE %d\n", it->second.type);
            break;
    }*/
    // Read all union cases
    uint addr = reader.m_addr;
    for (auto it = definition.members.begin(); it != definition.members.end(); it++) {
        reader.m_addr = addr; // All union fields read from the same memory addr
        switch (it->second.type) {
            case PLAIN:
            {
                Entry entry = readVar(reader, it->second.plainType);
                result.object[it->first] = entry;
                result.objectKeys.push_back(it->first);
                break;
            }
            case STRUCT:
            {
                Entry entry = readStruct(reader, it->second);
                result.object[it->first] = entry;
                result.objectKeys.push_back(it->first);
                break;
            }
            case UNION:
            {
                Entry entry = readUnion(reader, it->second);
                result.object[it->first] = entry;
                result.objectKeys.push_back(it->first);
                break;
            }
            default:
                printf("UNHANDLED DEFINITION TYPE %d\n", it->second.type);
                break;
        }
    }

    return result;
}

Entry EntityView::readBitfield(Reader& reader, uint count) {
    Entry entry;
    // TODO handle bitfields that don't fit into an u8?
    entry.type = U8;
    entry.u8 = reader.read_bitfield(count);
    return entry;
}

Entry EntityView::readVar(Reader& reader, const std::string& type) {
    //std::cout << "read " << type << " @" << reader.m_addr << std::endl;
    if (type.find("*") != std::string::npos) {
        Entry entry;
        entry.type = U32;
        entry.u32 = reader.read_u32();
        return entry;
    }

    if (type.find("[") != std::string::npos) {
        std::string innerType = type.substr(0, type.find("["));
        uint count = std::atoi(type.substr(type.find("[")+1, type.length()-1).c_str());
        return readArray(reader, innerType, count);
    }

    if (type.find(":") != std::string::npos) {
        uint count = std::atoi(type.substr(type.find(":")+1, type.length()).c_str());
        return readBitfield(reader, count);
    }
    if (type == "u8") {
        Entry entry;
        entry.type = U8;
        entry.u8 = reader.read_u8();
        return entry;
    } else if (type == "s8") {
        Entry entry;
        entry.type = S8;
        entry.s8 = reader.read_s8();
        return entry;
    } else if (type == "u16") {
        Entry entry;
        entry.type = U16;
        entry.u16 = reader.read_u16();
        return entry;
    } else if (type == "s16") {
        Entry entry;
        entry.type = S16;
        entry.s16 = reader.read_s16();
        return entry;
    } else if (type == "u32") {
        Entry entry;
        entry.type = U32;
        entry.u32 = reader.read_u32();
        return entry;
    } else if (type == "s32") {
        Entry entry;
        entry.type = S32;
        entry.s32 = reader.read_s32();
        return entry;
    }

    if (definitions.count(type) > 0) {
        if (definitions[type].type == STRUCT) {
            return readStruct(reader, definitions[type]);
        } else if (definitions[type].type == UNION) {
            return readUnion(reader, definitions[type]);
        }
    }
    Entry entry;
    entry.type = ERROR;
    entry.errorMessage = "TODO: " + type + " not found";
    return entry;
}

Entry EntityView::printVar(uint addr, const std::string& name, const std::string& type) {
    Reader reader(m_core, addr);
    Entry entry = readVar(reader, type);
    //printf("%s", name.c_str());
    printEntry(entry);
    return entry;
}


QString EntityView::printEntry(const Entry& entry, int indentation) {
    //printf("-%d-", entry.type);
    switch (entry.type) {
        case NONE:
            return "NONE";
        case ERROR:
            return QString("ERROR: %1").arg(QString(entry.errorMessage.c_str()));
        case U8:
            return QString("0x%1").arg(entry.u8, 1, 16);
        case S8:
            return QString("0x%1").arg(entry.s8, 1, 16);
        case U16:
            return QString("0x%1").arg(entry.u16, 1, 16);
        case S16:
            return QString("0x%1").arg(entry.s16, 1, 16);
        case U32:
            return QString("0x%1").arg(entry.u32, 1, 16);
        case S32:
            return QString("0x%1").arg(entry.s32, 1, 16);
        case OBJECT:
        {
            QString result = "{\n"+spaces(indentation+2);
            //for (auto it = entry.object.begin(); it != entry.object.end(); it++) {
            for (auto it = entry.objectKeys.begin(); it != entry.objectKeys.end(); it++) {
                //std::cout << "KEYS" << std::endl;
                //std::cout << it->c_str() << std::endl;
                if (it != entry.objectKeys.begin()) {
                    result += ", \n" + spaces(indentation+2);
                }
                result += QString("%1: ").arg(it->c_str());
                const Entry& res = entry.object.at(*it);
                result += printEntry(res, indentation + 2);
            }
            return result + "\n" + spaces(indentation)+"}";
        }
            break;
        case ARRAY:
        {
            QString result = "[\n"+spaces(indentation+2);
            for (int i = 0; i < entry.array.size(); i++) {
                if (i != 0) {
                    result += ", \n" + spaces(indentation+2);
                }
                result += printEntry(entry.array[i], indentation+2);
            }
            return result +"\n" + spaces(indentation)+ "]";
        }
            break;
        default:
            return QString("TODO %1\n").arg(entry.type);
            break;
    }
}

// TODO is there a way to do this more efficiently?
QString EntityView::spaces(int indentation) {
    QString result="";
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
    //return length;
    if (m_bitfieldRemaining == 0) {
        // Read the next byte
        m_bitfield = read_u8();
        m_bitfieldRemaining = 8;
    }

    if (m_bitfieldRemaining < length) {
        // Not enough bits remaining
        assert(false);
    }
    uint8_t val = m_bitfield & ((1<<(length))-1);
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


// Predefined warp points to avoid being stuck
// https://github.com/Straylite/MinishCap-Command-Box/blob/master/extension/warps.lua
struct WarpPoint {
    uint8_t Awarp;
    uint8_t Rwarp;
    uint16_t xwarp;
    uint16_t ywarp;
    uint8_t lwarp;
};
std::map<std::string, WarpPoint> warpPoints = {
    {"a0r0", {0, 0, 8, 35354, 1}},
    {"a103r0", {103, 0, 120, 80, 1} },
    {"a104r0", {104, 0, 120, 80, 1} },
    {"a104r1", {104, 1, 120, 80, 1} },
    {"a104r2", {104, 2, 150, 60, 1} },
    {"a104r3", {104, 3, 150, 60, 1} },
    {"a104r4", {104, 4, 150, 140, 1} },
    {"a104r5", {104, 5, 180, 105, 1} },
    {"a104r6", {104, 6, 160, 105, 1} },
    {"a104r7", {104, 7, 180, 105, 1} },
    {"a104r8", {104, 8, 180, 105, 1} },
    {"a10r0", {10, 0, 168, 216, 1} },
    {"a111r0", {111, 0, 170, 248, 2} },
    {"a112r0", {112, 0, 34171, 456, 1} },
    {"a112r1", {112, 1, 160, 80, 1} },
    {"a112r10", {112, 10, 160, 80, 1} },
    {"a112r11", {112, 11, 160, 80, 1} },
    {"a112r12", {112, 12, 160, 80, 1} },
    {"a112r13", {112, 13, 56, 72, 1} },
    {"a112r14", {112, 14, 56, 72, 1} },
    {"a112r15", {112, 15, 56, 130, 1} },
    {"a112r16", {112, 16, 56, 260, 1} },
    {"a112r17", {112, 17, 56, 180, 1} },
    {"a112r18", {112, 18, 33514, 136, 1} },
    {"a112r19", {112, 19, 33560, 110, 1} },
    {"a112r2", {112, 2, 160, 80, 1} },
    {"a112r20", {112, 20, 33590, 110, 1} },
    {"a112r21", {112, 21, 184, 232, 1} },
    {"a112r22", {112, 22, 33300, 200, 1} },
    {"a112r23", {112, 23, 16, 38189, 1} },
    {"a112r24", {112, 24, 160, 38540, 1} },
    {"a112r25", {112, 25, 350, 38400, 1} },
    {"a112r26", {112, 26, 350, 38400, 1} },
    {"a112r27", {112, 27, 184, 72, 1} },
    {"a112r28", {112, 28, 184, 72, 1} },
    {"a112r29", {112, 29, 184, 72, 1} },
    {"a112r3", {112, 3, 160, 80, 1} },
    {"a112r30", {112, 30, 184, 72, 1} },
    {"a112r31", {112, 31, 184, 100, 1} },
    {"a112r32", {112, 32, 184, 72, 1} },
    {"a112r33", {112, 33, 150, 250, 1} },
    {"a112r34", {112, 34, 150, 250, 1} },
    {"a112r35", {112, 35, 150, 235, 1} },
    {"a112r36", {112, 36, 100, 100, 1} },
    {"a112r37", {112, 37, 80, 104, 1} },
    {"a112r38", {112, 38, 80, 104, 1} },
    {"a112r39", {112, 39, 80, 104, 1} },
    {"a112r4", {112, 4, 160, 80, 1} },
    {"a112r40", {112, 40, 80, 104, 1} },
    {"a112r41", {112, 41, 80, 104, 1} },
    {"a112r42", {112, 42, 80, 104, 1} },
    {"a112r43", {112, 43, 80, 104, 1} },
    {"a112r44", {112, 44, 80, 200, 1} },
    {"a112r45", {112, 45, 130, 70, 1} },
    {"a112r46", {112, 46, 60, 170, 1} },
    {"a112r47", {112, 47, 120, 40, 1} },
    {"a112r48", {112, 48, 120, 40, 1} },
    {"a112r49", {112, 49, 570, 88, 1} },
    {"a112r5", {112, 5, 185, 80, 1} },
    {"a112r50", {112, 50, 570, 88, 1} },
    {"a112r6", {112, 6, 185, 80, 1} },
    {"a112r7", {112, 7, 185, 80, 1} },
    {"a112r8", {112, 8, 185, 80, 1} },
    {"a112r9", {112, 9, 160, 80, 1} },
    {"a113r0", {113, 0, 570, 88, 1} },
    {"a119r0", {119, 0, 605, 120, 1} },
    {"a11r0", {11, 0, 184, 515, 0} },
    {"a11r1", {11, 1, 904, 72, 1} },
    {"a120r0", {120, 0, 200, 488, 1} },
    {"a120r1", {120, 1, 200, 488, 1} },
    {"a120r2", {120, 2, 200, 100, 1} },
    {"a127r0", {127, 0, 200, 100, 1} },
    {"a128r0", {128, 0, 200, 100, 1} },
    {"a128r1", {128, 1, 200, 100, 1} },
    {"a128r2", {128, 2, 200, 100, 1} },
    {"a128r3", {128, 3, 200, 90, 1} },
    {"a128r4", {128, 4, 200, 90, 1} },
    {"a129r0", {129, 0, 182, 248, 2} },
    {"a12r0", {12, 0, 584, 424, 1} },
    {"a135r0", {135, 0, 210, 180, 1} },
    {"a136r0", {136, 0, 408, 496, 1} },
    {"a136r1", {136, 1, 168, 72, 1} },
    {"a136r10", {136, 10, 168, 72, 1} },
    {"a136r11", {136, 11, 168, 72, 1} },
    {"a136r12", {136, 12, 168, 72, 1} },
    {"a136r13", {136, 13, 168, 72, 1} },
    {"a136r14", {136, 14, 168, 72, 1} },
    {"a136r15", {136, 15, 168, 72, 1} },
    {"a136r16", {136, 16, 168, 72, 1} },
    {"a136r17", {136, 17, 140, 72, 1} },
    {"a136r18", {136, 18, 168, 72, 1} },
    {"a136r19", {136, 19, 168, 72, 1} },
    {"a136r2", {136, 2, 168, 72, 1} },
    {"a136r20", {136, 20, 168, 72, 1} },
    {"a136r21", {136, 21, 168, 72, 1} },
    {"a136r22", {136, 22, 168, 72, 1} },
    {"a136r23", {136, 23, 168, 72, 1} },
    {"a136r24", {136, 24, 168, 72, 1} },
    {"a136r25", {136, 25, 168, 72, 1} },
    {"a136r26", {136, 26, 168, 72, 1} },
    {"a136r27", {136, 27, 168, 72, 1} },
    {"a136r28", {136, 28, 168, 72, 1} },
    {"a136r29", {136, 29, 104, 280, 1} },
    {"a136r3", {136, 3, 168, 72, 1} },
    {"a136r30", {136, 30, 104, 72, 1} },
    {"a136r31", {136, 31, 136, 88, 1} },
    {"a136r32", {136, 32, 184, 72, 1} },
    {"a136r33", {136, 33, 184, 72, 1} },
    {"a136r34", {136, 34, 184, 72, 1} },
    {"a136r35", {136, 35, 160, 72, 1} },
    {"a136r36", {136, 36, 184, 72, 1} },
    {"a136r37", {136, 37, 184, 72, 1} },
    {"a136r38", {136, 38, 184, 120, 1} },
    {"a136r39", {136, 39, 184, 72, 1} },
    {"a136r4", {136, 4, 168, 72, 1} },
    {"a136r40", {136, 40, 184, 72, 1} },
    {"a136r41", {136, 41, 184, 72, 1} },
    {"a136r42", {136, 42, 160, 72, 1} },
    {"a136r43", {136, 43, 184, 72, 1} },
    {"a136r44", {136, 44, 184, 72, 1} },
    {"a136r45", {136, 45, 179, 72, 1} },
    {"a136r46", {136, 46, 184, 72, 1} },
    {"a136r47", {136, 47, 136, 120, 1} },
    {"a136r48", {136, 48, 136, 120, 1} },
    {"a136r49", {136, 49, 136, 120, 1} },
    {"a136r5", {136, 5, 168, 72, 1} },
    {"a136r50", {136, 50, 136, 120, 1} },
    {"a136r51", {136, 51, 70, 120, 2} },
    {"a136r52", {136, 52, 70, 120, 2} },
    {"a136r53", {136, 53, 70, 120, 2} },
    {"a136r54", {136, 54, 70, 100, 1} },
    {"a136r55", {136, 55, 408, 70, 1} },
    {"a136r57", {136, 57, 80, 100, 1} },
    {"a136r58", {136, 58, 140, 100, 1} },
    {"a136r6", {136, 6, 168, 72, 1} },
    {"a136r7", {136, 7, 168, 72, 1} },
    {"a136r8", {136, 8, 168, 90, 1} },
    {"a136r9", {136, 9, 168, 170, 1} },
    {"a137r0", {137, 0, 170, 100, 1} },
    {"a137r1", {137, 1, 136, 100, 1} },
    {"a137r2", {137, 2, 136, 216, 2} },
    {"a137r3", {137, 3, 136, 216, 2} },
    {"a137r4", {137, 4, 136, 216, 2} },
    {"a137r5", {137, 5, 120, 232, 2} },
    {"a137r6", {137, 6, 120, 232, 2} },
    {"a137r7", {137, 7, 120, 232, 2} },
    {"a139r0", {139, 0, 176, 136, 1} },
    {"a13r0", {13, 0, 88, 104, 1} },
    {"a13r1", {13, 1, 88, 104, 1} },
    {"a13r16", {13, 16, 120, 24, 1} },
    {"a13r17", {13, 17, 120, 24, 1} },
    {"a13r18", {13, 18, 120, 24, 1} },
    {"a13r19", {13, 19, 120, 24, 1} },
    {"a13r2", {13, 2, 88, 104, 1} },
    {"a13r20", {13, 20, 120, 24, 1} },
    {"a13r3", {13, 3, 88, 104, 1} },
    {"a13r4", {13, 4, 88, 104, 1} },
    {"a140r0", {140, 0, 176, 136, 2} },
    {"a141r0", {141, 0, 150, 160, 1} },
    {"a143r0", {143, 0, 150, 160, 1} },
    {"a15r0", {15, 0, 392, 520, 1} },
    {"a16r0", {16, 0, 120, 72, 1} },
    {"a17r0", {17, 0, 136, 776, 1} },
    {"a17r1", {17, 1, 120, 792, 1} },
    {"a17r2", {17, 2, 128, 792, 1} },
    {"a17r3", {17, 3, 128, 792, 1} },
    {"a17r4", {17, 4, 120, 792, 1} },
    {"a18r0", {18, 0, 128, 792, 1} },
    {"a18r1", {18, 1, 128, 792, 1} },
    {"a18r2", {18, 2, 8, 80, 1} },
    {"a18r3", {18, 3, 120, 168, 1} },
    {"a19r0", {19, 0, 100, 130, 1} },
    {"a19r3", {19, 3, 136, 104, 1} },
    {"a1r0", {1, 0, 600, 104, 1} },
    {"a1r1", {1, 1, 312, 120, 1} },
    {"a20r0", {20, 0, 40, 120, 1} },
    {"a21r0", {21, 0, 200, 952, 1} },
    {"a22r0", {22, 0, 295, 520, 1} },
    {"a23r0", {23, 0, 770, 360, 1} },
    {"a24r0", {24, 0, 472, 140, 1} },
    {"a24r1", {24, 1, 472, 168, 1} },
    {"a24r2", {24, 2, 472, 104, 1} },
    {"a24r3", {24, 3, 136, 176, 1} },
    {"a24r4", {24, 4, 88, 164, 1} },
    {"a25r0", {25, 0, 135, 125, 1} },
    {"a25r1", {25, 1, 57, 140, 1} },
    {"a26r0", {26, 0, 88, 72, 1} },
    {"a2r0", {2, 0, 504, 24, 1} },
    {"a32r0", {32, 0, 120, 120, 1} },
    {"a32r1", {32, 1, 104, 80, 1} },
    {"a32r16", {32, 16, 120, 40, 1} },
    {"a32r17", {32, 17, 120, 40, 1} },
    {"a32r18", {32, 18, 36, 86, 1} },
    {"a32r2", {32, 2, 232, 72, 1} },
    {"a32r3", {32, 3, 128, 120, 1} },
    {"a32r32", {32, 32, 120, 120, 1} },
    {"a32r33", {32, 33, 120, 120, 1} },
    {"a32r34", {32, 34, 120, 120, 1} },
    {"a32r35", {32, 35, 120, 120, 1} },
    {"a32r36", {32, 36, 120, 120, 1} },
    {"a32r37", {32, 37, 120, 120, 1} },
    {"a32r38", {32, 38, 120, 120, 1} },
    {"a32r39", {32, 39, 120, 120, 1} },
    {"a32r4", {32, 4, 128, 120, 1} },
    {"a32r5", {32, 5, 128, 120, 1} },
    {"a32r6", {32, 6, 128, 120, 1} },
    {"a32r7", {32, 7, 120, 120, 1} },
    {"a32r8", {32, 8, 120, 200, 1} },
    {"a32r9", {32, 9, 120, 320, 1} },
    {"a33r0", {33, 0, 104, 152, 1} },
    {"a33r1", {33, 1, 120, 144, 1} },
    {"a33r10", {33, 10, 152, 36, 1} },
    {"a33r11", {33, 11, 120, 200, 1} },
    {"a33r12", {33, 12, 216, 136, 1} },
    {"a33r2", {33, 2, 120, 136, 1} },
    {"a33r3", {33, 3, 104, 40, 1} },
    {"a33r4", {33, 4, 104, 120, 1} },
    {"a33r5", {33, 5, 152, 120, 1} },
    {"a33r6", {33, 6, 120, 120, 1} },
    {"a33r7", {33, 7, 120, 120, 1} },
    {"a33r8", {33, 8, 104, 120, 1} },
    {"a33r9", {33, 9, 72, 424, 1} },
    {"a34r0", {34, 0, 120, 120, 1} },
    {"a34r1", {34, 1, 120, 152, 1} },
    {"a34r10", {34, 10, 120, 120, 1} },
    {"a34r11", {34, 11, 120, 120, 1} },
    {"a34r12", {34, 12, 120, 120, 1} },
    {"a34r16", {34, 16, 120, 120, 1} },
    {"a34r17", {34, 17, 120, 120, 1} },
    {"a34r18", {34, 18, 120, 120, 1} },
    {"a34r19", {34, 19, 120, 140, 1} },
    {"a34r2", {34, 2, 120, 152, 1} },
    {"a34r20", {34, 20, 120, 120, 1} },
    {"a34r21", {34, 21, 88, 40, 1} },
    {"a34r3", {34, 3, 120, 152, 1} },
    {"a34r4", {34, 4, 120, 152, 1} },
    {"a34r5", {34, 5, 120, 152, 1} },
    {"a34r6", {34, 6, 120, 120, 1} },
    {"a34r7", {34, 7, 120, 152, 1} },
    {"a34r8", {34, 8, 120, 136, 1} },
    {"a34r9", {34, 9, 120, 120, 1} },
    {"a35r0", {35, 0, 120, 168, 1} },
    {"a35r1", {35, 1, 120, 168, 1} },
    {"a35r2", {35, 2, 200, 96, 1} },
    {"a35r3", {35, 3, 40, 144, 1} },
    {"a35r4", {35, 4, 200, 112, 1} },
    {"a35r5", {35, 5, 200, 96, 1} },
    {"a35r6", {35, 6, 120, 136, 1} },
    {"a35r7", {35, 7, 120, 120, 1} },
    {"a35r8", {35, 8, 120, 120, 1} },
    {"a36r0", {36, 0, 120, 136, 1} },
    {"a36r16", {36, 16, 120, 120, 1} },
    {"a36r17", {36, 17, 120, 120, 1} },
    {"a36r18", {36, 18, 120, 120, 1} },
    {"a36r19", {36, 19, 120, 120, 1} },
    {"a36r20", {36, 20, 120, 120, 1} },
    {"a36r21", {36, 21, 120, 120, 1} },
    {"a36r22", {36, 22, 120, 120, 1} },
    {"a36r23", {36, 23, 120, 120, 1} },
    {"a36r24", {36, 24, 120, 120, 1} },
    {"a36r25", {36, 25, 120, 120, 1} },
    {"a36r26", {36, 26, 120, 120, 1} },
    {"a36r27", {36, 27, 120, 120, 1} },
    {"a36r28", {36, 28, 140, 120, 1} },
    {"a36r29", {36, 29, 120, 120, 1} },
    {"a36r30", {36, 30, 135, 120, 1} },
    {"a36r31", {36, 31, 120, 120, 1} },
    {"a37r0", {37, 0, 120, 160, 1} },
    {"a37r1", {37, 1, 120, 160, 1} },
    {"a37r10", {37, 10, 120, 104, 1} },
    {"a37r11", {37, 11, 120, 152, 1} },
    {"a37r12", {37, 12, 120, 152, 1} },
    {"a37r13", {37, 13, 120, 152, 1} },
    {"a37r2", {37, 2, 120, 160, 1} },
    {"a37r3", {37, 3, 120, 160, 1} },
    {"a37r4", {37, 4, 120, 160, 1} },
    {"a37r5", {37, 5, 656, 412, 1} },
    {"a37r6", {37, 6, 120, 152, 1} },
    {"a37r7", {37, 7, 120, 152, 1} },
    {"a37r8", {37, 8, 120, 152, 1} },
    {"a37r9", {37, 9, 120, 152, 1} },
    {"a38r0", {38, 0, 664, 440, 1} },
    {"a38r1", {38, 1, 184, 136, 1} },
    {"a38r10", {38, 10, 104, 40, 1} },
    {"a38r11", {38, 11, 184, 72, 1} },
    {"a38r12", {38, 12, 120, 136, 1} },
    {"a38r13", {38, 13, 120, 120, 1} },
    {"a38r14", {38, 14, 120, 120, 1} },
    {"a38r15", {38, 15, 120, 120, 1} },
    {"a38r16", {38, 16, 120, 240, 1} },
    {"a38r2", {38, 2, 72, 456, 1} },
    {"a38r3", {38, 3, 184, 152, 1} },
    {"a38r4", {38, 4, 120, 120, 1} },
    {"a38r5", {38, 5, 120, 120, 1} },
    {"a38r6", {38, 6, 56, 152, 1} },
    {"a38r7", {38, 7, 184, 136, 1} },
    {"a38r8", {38, 8, 120, 280, 1} },
    {"a38r9", {38, 9, 120, 120, 1} },
    {"a39r0", {39, 0, 120, 56, 1} },
    {"a39r1", {39, 1, 120, 56, 1} },
    {"a39r10", {39, 10, 120, 56, 1} },
    {"a39r11", {39, 11, 120, 56, 1} },
    {"a39r12", {39, 12, 120, 56, 1} },
    {"a39r13", {39, 13, 120, 56, 1} },
    {"a39r14", {39, 14, 120, 56, 1} },
    {"a39r15", {39, 15, 120, 56, 1} },
    {"a39r16", {39, 16, 120, 56, 1} },
    {"a39r17", {39, 17, 120, 56, 1} },
    {"a39r3", {39, 3, 120, 56, 1} },
    {"a39r4", {39, 4, 120, 56, 1} },
    {"a39r5", {39, 5, 120, 56, 1} },
    {"a39r6", {39, 6, 120, 56, 1} },
    {"a39r7", {39, 7, 120, 56, 1} },
    {"a39r8", {39, 8, 120, 56, 1} },
    {"a39r9", {39, 9, 120, 56, 1} },
    {"a3r0", {3, 0, 184, 53, 1} },
    {"a3r1", {3, 1, 656, 412, 1} },
    {"a3r2", {3, 2, 472, 160, 1} },
    {"a3r3", {3, 3, 168, 168, 1} },
    {"a3r4", {3, 4, 472, 35350, 1} },
    {"a3r5", {3, 5, 344, 652, 1} },
    {"a3r6", {3, 6, 504, 312, 1} },
    {"a3r7", {3, 7, 472, 560, 1} },
    {"a3r8", {3, 8, 160, 504, 1} },
    {"a3r9", {3, 9, 144, 92, 1} },
    {"a40r0", {40, 0, 120, 152, 1} },
    {"a40r1", {40, 1, 120, 152, 1} },
    {"a40r2", {40, 2, 104, 120, 1} },
    {"a40r3", {40, 3, 120, 120, 1} },
    {"a40r4", {40, 4, 120, 136, 1} },
    {"a40r5", {40, 5, 120, 160, 1} },
    {"a41r0", {41, 0, 120, 248, 1} },
    {"a41r1", {41, 1, 120, 248, 1} },
    {"a41r2", {41, 2, 120, 248, 1} },
    {"a42r0", {42, 0, 120, 152, 1} },
    {"a42r1", {42, 1, 56, 120, 1} },
    {"a42r2", {42, 2, 120, 120, 1} },
    {"a42r3", {42, 3, 136, 120, 1} },
    {"a42r4", {42, 4, 120, 120, 1} },
    {"a43r0", {43, 0, 120, 120, 1} },
    {"a43r1", {43, 1, 392, 40, 1} },
    {"a44r0", {44, 0, 120, 128, 1} },
    {"a44r1", {44, 1, 120, 128, 1} },
    {"a44r10", {44, 10, 120, 128, 1} },
    {"a44r2", {44, 2, 120, 128, 1} },
    {"a44r3", {44, 3, 120, 128, 1} },
    {"a44r4", {44, 4, 120, 128, 1} },
    {"a44r5", {44, 5, 120, 128, 1} },
    {"a44r6", {44, 6, 120, 128, 1} },
    {"a44r7", {44, 7, 120, 128, 1} },
    {"a44r8", {44, 8, 120, 128, 1} },
    {"a44r9", {44, 9, 120, 128, 1} },
    {"a45r0", {45, 0, 120, 120, 1} },
    {"a45r1", {45, 1, 120, 120, 1} },
    {"a45r16", {45, 16, 168, 424, 1} },
    {"a45r17", {45, 17, 120, 200, 1} },
    {"a45r18", {45, 18, 280, 271, 1} },
    {"a45r2", {45, 2, 120, 120, 1} },
    {"a45r3", {45, 3, 120, 120, 1} },
    {"a45r4", {45, 4, 120, 120, 1} },
    {"a45r5", {45, 5, 120, 120, 1} },
    {"a46r0", {46, 0, 56, 40, 1} },
    {"a46r1", {46, 1, 40, 192, 1} },
    {"a46r2", {46, 2, 232, 40, 1} },
    {"a46r3", {46, 3, 328, 40, 1} },
    {"a47r0", {47, 0, 120, 120, 1} },
    {"a47r1", {47, 1, 120, 632, 1} },
    {"a48r0", {48, 0, 120, 312, 1} },
    {"a48r1", {48, 1, 184, 248, 1} },
    {"a48r2", {48, 2, 184, 248, 1} },
    {"a48r3", {48, 3, 184, 248, 1} },
    {"a49r0", {49, 0, 184, 328, 1} },
    {"a4r0", {4, 0, 1000, 35017, 1} },
    {"a50r0", {50, 0, 168, 216, 1} },
    {"a50r1", {50, 1, 264, 216, 1} },
    {"a50r10", {50, 10, 120, 120, 1} },
    {"a50r11", {50, 11, 120, 120, 1} },
    {"a50r12", {50, 12, 56, 40, 1} },
    {"a50r13", {50, 13, 56, 90, 1} },
    {"a50r14", {50, 14, 56, 56, 1} },
    {"a50r15", {50, 15, 120, 120, 1} },
    {"a50r16", {50, 16, 120, 120, 1} },
    {"a50r17", {50, 17, 120, 120, 1} },
    {"a50r18", {50, 18, 120, 120, 1} },
    {"a50r19", {50, 19, 120, 120, 1} },
    {"a50r20", {50, 20, 120, 120, 1} },
    {"a50r21", {50, 21, 120, 72, 1} },
    {"a50r22", {50, 22, 120, 120, 1} },
    {"a50r23", {50, 23, 120, 120, 1} },
    {"a50r7", {50, 7, 120, 120, 1} },
    {"a50r8", {50, 8, 120, 120, 1} },
    {"a50r9", {50, 9, 120, 120, 1} },
    {"a51r0", {51, 0, 56, 120, 1} },
    {"a51r1", {51, 1, 184, 120, 1} },
    {"a51r2", {51, 2, 150, 100, 1} },
    {"a51r3", {51, 3, 56, 120, 1} },
    {"a51r4", {51, 4, 152, 120, 1} },
    {"a51r5", {51, 5, 88, 72, 1} },
    {"a51r6", {51, 6, 152, 72, 1} },
    {"a51r7", {51, 7, 152, 72, 1} },
    {"a51r8", {51, 8, 152, 40, 1} },
    {"a52r0", {52, 0, 120, 120, 1} },
    {"a52r1", {52, 1, 120, 280, 1} },
    {"a53r0", {53, 0, 152, 424, 1} },
    {"a53r1", {53, 1, 120, 504, 1} },
    {"a53r2", {53, 2, 120, 200, 1} },
    {"a53r3", {53, 3, 328, 184, 1} },
    {"a53r4", {53, 4, 120, 184, 1} },
    {"a53r5", {53, 5, 120, 264, 1} },
    {"a53r6", {53, 6, 120, 250, 1} },
    {"a53r7", {53, 7, 392, 424, 1} },
    {"a53r8", {53, 8, 72, 104, 1} },
    {"a53r9", {53, 9, 88, 280, 1} },
    {"a54r0", {54, 0, 120, 184, 1} },
    {"a54r1", {54, 1, 120, 184, 1} },
    {"a55r0", {55, 0, 120, 184, 1} },
    {"a55r1", {55, 1, 120, 184, 1} },
    {"a56r0", {56, 0, 120, 184, 1} },
    {"a5r0", {5, 0, 32825, 16, 1} },
    {"a5r1", {5, 1, 72, 95, 1} },
    {"a5r2", {5, 2, 200, 84, 1} },
    {"a5r3", {5, 3, 56, 35, 1} },
    {"a5r4", {5, 4, 408, 40, 1} },
    {"a5r5", {5, 5, 168, 67, 0} },
    {"a64r0", {64, 0, 230, 210, 1} },
    {"a65r0", {65, 0, 360, 40, 1} },
    {"a65r1", {65, 1, 120, 80, 1} },
    {"a66r0", {66, 0, 120, 120, 1} },
    {"a66r1", {66, 1, 120, 120, 1} },
    {"a67r0", {67, 0, 120, 120, 1} },
    {"a67r1", {67, 1, 40, 90, 1} },
    {"a68r0", {68, 0, 152, 136, 1} },
    {"a69r0", {69, 0, 60, 60, 1} },
    {"a6r0", {6, 0, 152, 65, 1} },
    {"a6r1", {6, 1, 168, 104, 1} },
    {"a6r2", {6, 2, 488, 488, 1} },
    {"a6r3", {6, 3, 168, 88, 1} },
    {"a6r4", {6, 4, 312, 328, 1} },
    {"a70r0", {70, 0, 60, 60, 1} },
    {"a70r1", {70, 1, 60, 60, 1} },
    {"a70r2", {70, 2, 60, 60, 1} },
    {"a70r3", {70, 3, 60, 60, 1} },
    {"a70r4", {70, 4, 130, 60, 1} },
    {"a70r5", {70, 5, 130, 110, 1} },
    {"a70r6", {70, 6, 130, 60, 1} },
    {"a70r7", {70, 7, 130, 60, 1} },
    {"a71r0", {71, 0, 130, 60, 1} },
    {"a71r1", {71, 1, 130, 60, 1} },
    {"a71r2", {71, 2, 160, 90, 1} },
    {"a72r0", {72, 0, 160, 90, 1} },
    {"a72r1", {72, 1, 160, 90, 1} },
    {"a72r10", {72, 10, 140, 70, 1} },
    {"a72r11", {72, 11, 140, 70, 1} },
    {"a72r16", {72, 16, 140, 70, 1} },
    {"a72r17", {72, 17, 140, 70, 1} },
    {"a72r18", {72, 18, 140, 70, 1} },
    {"a72r19", {72, 19, 140, 70, 1} },
    {"a72r2", {72, 2, 160, 90, 1} },
    {"a72r20", {72, 20, 100, 70, 1} },
    {"a72r21", {72, 21, 100, 70, 1} },
    {"a72r23", {72, 23, 100, 70, 1} },
    {"a72r3", {72, 3, 160, 90, 1} },
    {"a72r32", {72, 32, 100, 70, 1} },
    {"a72r4", {72, 4, 160, 90, 1} },
    {"a72r5", {72, 5, 160, 90, 1} },
    {"a72r6", {72, 6, 160, 90, 1} },
    {"a72r7", {72, 7, 140, 70, 1} },
    {"a72r8", {72, 8, 140, 70, 1} },
    {"a72r9", {72, 9, 140, 70, 1} },
    {"a73r0", {73, 0, 100, 70, 1} },
    {"a74r0", {74, 0, 120, 100, 1} },
    {"a77r0", {77, 0, 120, 70, 1} },
    {"a7r0", {7, 0, 504, 56, 1} },
    {"a80r0", {80, 0, 136, 72, 1} },
    {"a80r1", {80, 1, 136, 72, 1} },
    {"a80r16", {80, 16, 136, 52, 1} },
    {"a80r17", {80, 17, 400, 52, 1} },
    {"a80r18", {80, 18, 33676, 192, 1} },
    {"a80r19", {80, 19, 136, 55, 1} },
    {"a80r2", {80, 2, 136, 72, 1} },
    {"a80r20", {80, 20, 136, 55, 1} },
    {"a80r21", {80, 21, 136, 55, 1} },
    {"a80r22", {80, 22, 136, 55, 1} },
    {"a80r23", {80, 23, 33670, 40, 1} },
    {"a80r3", {80, 3, 136, 72, 1} },
    {"a80r4", {80, 4, 136, 72, 1} },
    {"a80r5", {80, 5, 136, 72, 1} },
    {"a80r6", {80, 6, 136, 72, 1} },
    {"a80r7", {80, 7, 136, 52, 1} },
    {"a80r8", {80, 8, 136, 52, 1} },
    {"a80r9", {80, 9, 136, 52, 1} },
    {"a81r0", {81, 0, 192, 248, 2} },
    {"a87r0", {87, 0, 182, 248, 2} },
    {"a88r0", {88, 0, 182, 150, 1} },
    {"a88r1", {88, 1, 33802, 200, 1} },
    {"a88r16", {88, 16, 120, 160, 1} },
    {"a88r17", {88, 17, 60, 160, 1} },
    {"a88r18", {88, 18, 45, 85, 1} },
    {"a88r19", {88, 19, 45, 85, 1} },
    {"a88r2", {88, 2, 408, 376, 1} },
    {"a88r20", {88, 20, 45, 85, 1} },
    {"a88r21", {88, 21, 45, 85, 1} },
    {"a88r22", {88, 22, 33802, 200, 1} },
    {"a88r23", {88, 23, 136, 176, 1} },
    {"a88r24", {88, 24, 136, 140, 1} },
    {"a88r25", {88, 25, 136, 140, 1} },
    {"a88r26", {88, 26, 136, 140, 1} },
    {"a88r27", {88, 27, 136, 140, 1} },
    {"a88r28", {88, 28, 136, 140, 1} },
    {"a88r29", {88, 29, 136, 140, 1} },
    {"a88r3", {88, 3, 120, 160, 1} },
    {"a88r32", {88, 32, 136, 140, 1} },
    {"a88r33", {88, 33, 136, 140, 1} },
    {"a88r34", {88, 34, 136, 140, 1} },
    {"a88r35", {88, 35, 136, 140, 1} },
    {"a88r36", {88, 36, 136, 140, 1} },
    {"a88r4", {88, 4, 120, 160, 1} },
    {"a89r0", {89, 0, 160, 396, 1} },
    {"a8r0", {8, 0, 488, 360, 1} },
    {"a8r1", {8, 1, 776, 450, 1} },
    {"a8r2", {8, 2, 488, 472, 1} },
    {"a95r0", {95, 0, 184, 120, 1} },
    {"a96r0", {96, 0, 184, 120, 1} },
    {"a96r1", {96, 1, 184, 120, 1} },
    {"a96r10", {96, 10, 150, 120, 1} },
    {"a96r11", {96, 11, 150, 120, 1} },
    {"a96r12", {96, 12, 150, 120, 1} },
    {"a96r13", {96, 13, 150, 120, 1} },
    {"a96r14", {96, 14, 150, 120, 1} },
    {"a96r15", {96, 15, 150, 120, 1} },
    {"a96r16", {96, 16, 184, 120, 1} },
    {"a96r17", {96, 17, 150, 120, 1} },
    {"a96r2", {96, 2, 184, 120, 1} },
    {"a96r3", {96, 3, 184, 120, 1} },
    {"a96r32", {96, 32, 150, 120, 1} },
    {"a96r33", {96, 33, 150, 120, 1} },
    {"a96r34", {96, 34, 135, 120, 1} },
    {"a96r35", {96, 35, 135, 120, 1} },
    {"a96r36", {96, 36, 110, 120, 1} },
    {"a96r37", {96, 37, 135, 120, 1} },
    {"a96r38", {96, 38, 175, 120, 1} },
    {"a96r39", {96, 39, 175, 120, 1} },
    {"a96r4", {96, 4, 184, 120, 1} },
    {"a96r40", {96, 40, 175, 120, 1} },
    {"a96r41", {96, 41, 140, 120, 1} },
    {"a96r42", {96, 42, 140, 120, 1} },
    {"a96r43", {96, 43, 140, 120, 1} },
    {"a96r44", {96, 44, 140, 120, 1} },
    {"a96r45", {96, 45, 140, 120, 1} },
    {"a96r46", {96, 46, 136, 40, 1} },
    {"a96r47", {96, 47, 136, 40, 1} },
    {"a96r48", {96, 48, 136, 120, 1} },
    {"a96r49", {96, 49, 56, 24, 1} },
    {"a96r5", {96, 5, 184, 120, 1} },
    {"a96r50", {96, 50, 88, 104, 1} },
    {"a96r51", {96, 51, 88, 104, 1} },
    {"a96r52", {96, 52, 88, 104, 1} },
    {"a96r53", {96, 53, 108, 104, 1} },
    {"a96r54", {96, 54, 108, 104, 1} },
    {"a96r6", {96, 6, 184, 120, 1} },
    {"a96r7", {96, 7, 184, 120, 1} },
    {"a96r8", {96, 8, 184, 120, 1} },
    {"a96r9", {96, 9, 184, 120, 1} },
    {"a97r0", {97, 0, 120, 100, 1} },
    {"a98r0", {98, 0, 120, 100, 1} },
    {"a98r1", {98, 1, 120, 100, 1} },
    {"a98r16", {98, 16, 120, 100, 1} },
    {"a98r17", {98, 17, 120, 100, 1} },
    {"a98r18", {98, 18, 120, 100, 1} },
    {"a98r19", {98, 19, 120, 100, 1} },
    {"a98r2", {98, 2, 120, 100, 1} },
    {"a98r20", {98, 20, 120, 100, 1} },
    {"a98r21", {98, 21, 120, 100, 1} },
    {"a98r3", {98, 3, 120, 100, 1} },
    {"a98r4", {98, 4, 120, 100, 1} },
    {"a9r0", {9, 0, 472, 608, 1} },
    {"a9r1", {9, 1, 120, 152, 1 }}
};

void EntityView::slotCheatTeleport() {
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