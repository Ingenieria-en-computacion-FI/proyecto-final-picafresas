#ifndef OBJECT_H
#define OBJECT_H

#include <stdint.h>
#include "assembler.h"

#define OBJ_MAGIC "IA32OBJ"

typedef struct {
    char magic[8];
    uint32_t num_sections;
    uint32_t num_symbols;
    uint32_t num_relocations;
} ObjHeader;

typedef struct {
    char name[16];
    uint32_t type;
    uint32_t size;
    uint32_t offset;
    uint32_t base_addr;
} ObjSectionHeader;

typedef struct {
    char name[64];
    uint32_t value;
    uint32_t section;
    uint32_t type;
    uint32_t defined;
} ObjSymbol;

typedef struct {
    char symbol[64];
    uint32_t section;
    uint32_t offset;
    uint32_t type;
    int32_t addend;
} ObjRelocation;

int write_object_file(const char *filename, const AssemblerState *as);

int read_object_file(const char *filename, AssemblerState *as);

#endif
