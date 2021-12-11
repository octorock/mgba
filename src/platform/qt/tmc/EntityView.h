#pragma once

#include "ui_EntityView.h"

#include <map>
#include <string>
#include "EntityListModel.h"
#include "MemoryWatchModel.h"
struct mCore;
#include "rapidjson/document.h"
using std::string;

namespace QGBA {

class CoreController;

class Reader {
public:
    Reader(mCore* core, uint addr): m_addr(addr), m_core(core) {
    }

    uint8_t read_u8();

    int8_t read_s8() {
        int8_t val = read_u8();
        return val;
    }

    uint16_t read_u16();

    int16_t read_s16() {
        return read_u16();
    }

    uint32_t read_u32();

    int32_t read_s32() {
        return read_u32();
    }

    uint8_t read_bitfield(uint length);

    uint m_addr;
private:
    mCore* m_core;
    uint m_bitfield = 0;
    uint m_bitfieldRemaining = 0;
};


enum class Type {
    STRUCT,
    UNION,
    PLAIN
};

struct Definition {
    Type type;
    std::vector<std::pair<std::string, Definition>> members;
    std::string plainType;
};

enum class EntryType {
    NONE,
    ERROR,
    U8,
    S8,
    U16,
    S16,
    U32,
    S32,
    OBJECT,
    ARRAY
};

struct Entry {
    EntryType type;
    // TODO somehow make this a union
    std::string errorMessage;
    uint8_t u8;
    int8_t s8;
    uint16_t u16;
    int16_t s16;
    uint32_t u32;
    int32_t s32;
    std::map<std::string, Entry> object;
    std::vector<std::string> objectKeys;
    std::vector<Entry> array;

    /*Entry(const Entry& entry) : type(entry.type) {
        switch(entry.type) {
            case ERROR:
                new(&value.errorMessage)(entry.value.errorMessage);
                break;
        }
    }*/
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
    void slotCheatTeleport();
    void slotAddMemoryWatch();
private:
    Definition buildDefinition(const rapidjson::Value& value);
    
    Entry readVar(uint addr, const std::string& type);
    Entry readVar(Reader& reader, const std::string& type);
    Entry readArray(Reader& reader, const std::string& type, uint count);
    Entry readStruct(Reader& reader, const Definition& definition);
    Entry readUnion(Reader& reader, const Definition& definition);
    Entry readBitfield(Reader& reader, uint count);
    QString printEntry(const Entry& entry, int indentation=0);
    QString spaces(int indentation);



    Ui::EntityView m_ui;
    mCore* m_core = nullptr;
    std::map<std::string, Definition> definitions;
    EntityListModel m_model;
    EntityData m_currentEntity = {0};
    MemoryWatchModel m_memoryModel;
    MemoryWatch m_currentWatch = {0};
    std::shared_ptr<CoreController> m_context = nullptr;
    QImage m_backing;

    QPen m_hitboxPen;
    QPen m_circlePen;
    QPen m_linePen;
};
}



