#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <stdint.h>
#include "parser.h"

#define MAX_SYMBOLS 512
#define MAX_FIXUPS 256
#define MAX_SECTION_SIZE 65536
#define MAX_SECTIONS 3

typedef enum {
    SEC_TEXT = 0,
    SEC_DATA = 1,
    SEC_BSS = 2
} SectionType;

typedef struct {
    SectionType type;
    const char *name;
    uint8_t data[MAX_SECTION_SIZE];
    int size;
    uint32_t base_addr;
} Section;

typedef enum {
    SYM_LOCAL,
    SYM_GLOBAL,
    SYM_EXTERN,
    SYM_EQU
} SymbolType;

typedef struct {
    char name[64];
    uint32_t value;
    SectionType section;
    SymbolType type;
    int defined;
} Symbol;

typedef enum {
    FIX_REL32,
    FIX_ABS32
} FixupType;

typedef struct {
    FixupType type;
    char symbol[64];
    SectionType section;
    int offset;
    int instr_end;
} Fixup;

typedef struct {
    Section sections[MAX_SECTIONS];
    SectionType current_section;

    Symbol symbols[MAX_SYMBOLS];
    int num_symbols;

    Fixup fixups[MAX_FIXUPS];
    int num_fixups;

    int errors;
    int warnings;
} AssemblerState;

void assembler_init(AssemblerState *as);

int assemble_one_pass(AssemblerState *as, const ParseResult *ast);

int assemble_two_pass(AssemblerState *as, const ParseResult *ast);

void print_symbol_table(const AssemblerState *as);

void print_section_hex(const AssemblerState *as, SectionType sec);

void assembler_free(AssemblerState *as);

Symbol *symbol_lookup(AssemblerState *as, const char *name);

int symbol_define(AssemblerState *as, const char *name,
                  uint32_t value, SectionType sec, SymbolType type);

void emit_byte(AssemblerState *as, uint8_t byte);

void emit_bytes(AssemblerState *as, const uint8_t *data, int n);

void emit_u32(AssemblerState *as, uint32_t val);

int current_offset(const AssemblerState *as);

#endif