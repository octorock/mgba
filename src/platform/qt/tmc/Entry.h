#pragma once
#include <string>
#include <vector>
#include <map>

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

// All my homies hate WinGDI.h for defining ERROR: https://stackoverflow.com/a/27064722
#define NOGDI
#undef ERROR

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
    uint addr;

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
};