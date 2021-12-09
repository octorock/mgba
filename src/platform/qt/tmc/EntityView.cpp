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

    FILE* fp = fopen("../src/platform/qt/tmc/structs.json", "rb");
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
        std::vector<Entry> entities;
        uint32_t first = it.object["first"].u32;
        if (first != 0 && first != listAddr + 8 * i) {
            // There are elements in this
            uint32_t next = first;
            uint j = 0;
            do {
                uint8_t kind = m_core->rawRead8(m_core, next+8, -1);
                uint8_t id = m_core->rawRead8(m_core, next+9, -1);
                Entry entity;
                Reader reader = Reader(m_core, next);
                if (kind == 9) {
                    entity = readVar(reader, "Manager");
                } else {
                    entity = readVar(reader, "Entity");
                }
                //printEntry(entity);
                entities.push_back(entity);
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
                next = entity.object["next"].u32;
                uint32_t prev =  entity.object["next"].u32;
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
