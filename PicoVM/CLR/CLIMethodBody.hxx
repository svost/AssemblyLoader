#ifndef __CLIMETHODBODY_HXX__
#define __CLIMETHODBODY_HXX__

#include <cstdint>
#include <vector>

#include "CLIMetadataTableRows.hxx"

enum struct MethodBodyFlags : uint8_t {
    TinyFormat = 0x2, // Method header is Tiny.
    FatFormat = 0x3,  // Method header is Fat.
    MoreSects = 0x8,  // More sections follow after this header.
    InitLocals = 0x10,// Call default constructor on all local variables.
};

struct ExceptionClause {
    uint16_t flags = 0;
    uint16_t tryOffset = 0;
    uint16_t handlerOffset = 0;
    uint16_t handlerlength = 0;
    uint32_t classTokenOrFilterOffset = 0;
};

enum struct ExceptionClauseFlags : uint8_t {
    EHTable = 0x01,   // It's exception handling section
    FatFormat = 0x40, // Section uses Fat format
    MoreSects = 0x80, // More sections to follow
};

struct MethodBody {
    MethodDefRow methodDef;
    std::vector<uint8_t> data;
    std::vector<ExceptionClause> exceptions;
    uint32_t maxStack = 0;
    uint32_t localVarSigTok = 0;
    bool initLocals = false;
};

#endif