#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "object.h"

int write_object_file(const char *filename, const AssemblerState *as) {
    FILE *f = fopen(filename, "wb");
    if (!f) {
        perror("[object] fopen write failed");
        return -1;
    }

    ObjHeader header;
    memset(&header, 0, sizeof(header));
    memcpy(header.magic, OBJ_MAGIC, sizeof(header.magic));
    header.num_sections = MAX_SECTIONS;
    header.num_symbols = as->num_symbols;
    header.num_relocations = as->num_fixups;

    uint32_t header_sz = sizeof(ObjHeader);
    uint32_t sec_headers_sz = MAX_SECTIONS * sizeof(ObjSectionHeader);
    uint32_t symbols_sz = header.num_symbols * sizeof(ObjSymbol);
    uint32_t relocs_sz = header.num_relocations * sizeof(ObjRelocation);

    uint32_t text_offset = header_sz + sec_headers_sz + symbols_sz + relocs_sz;
    uint32_t data_offset = text_offset + as->sections[SEC_TEXT].size;

    ObjSectionHeader sec_hdrs[MAX_SECTIONS];
    memset(sec_hdrs, 0, sizeof(sec_hdrs));

    strncpy(sec_hdrs[SEC_TEXT].name, ".text", sizeof(sec_hdrs[SEC_TEXT].name) - 1);
    sec_hdrs[SEC_TEXT].type = SEC_TEXT;
    sec_hdrs[SEC_TEXT].size = as->sections[SEC_TEXT].size;
    sec_hdrs[SEC_TEXT].offset = text_offset;
    sec_hdrs[SEC_TEXT].base_addr = as->sections[SEC_TEXT].base_addr;

    strncpy(sec_hdrs[SEC_DATA].name, ".data", sizeof(sec_hdrs[SEC_DATA].name) - 1);
    sec_hdrs[SEC_DATA].type = SEC_DATA;
    sec_hdrs[SEC_DATA].size = as->sections[SEC_DATA].size;
    sec_hdrs[SEC_DATA].offset = data_offset;
    sec_hdrs[SEC_DATA].base_addr = as->sections[SEC_DATA].base_addr;

    strncpy(sec_hdrs[SEC_BSS].name, ".bss", sizeof(sec_hdrs[SEC_BSS].name) - 1);
    sec_hdrs[SEC_BSS].type = SEC_BSS;
    sec_hdrs[SEC_BSS].size = as->sections[SEC_BSS].size;
    sec_hdrs[SEC_BSS].offset = 0;
    sec_hdrs[SEC_BSS].base_addr = as->sections[SEC_BSS].base_addr;

    if (fwrite(&header, sizeof(ObjHeader), 1, f) != 1) {
        fprintf(stderr, "[object] Error escribiendo cabecera\n");
        fclose(f);
        return -1;
    }

    if (fwrite(sec_hdrs, sizeof(ObjSectionHeader), MAX_SECTIONS, f) != MAX_SECTIONS) {
        fprintf(stderr, "[object] Error escribiendo cabeceras de seccion\n");
        fclose(f);
        return -1;
    }

    for (int i = 0; i < as->num_symbols; i++) {
        ObjSymbol sym;
        memset(&sym, 0, sizeof(sym));
        strncpy(sym.name, as->symbols[i].name, sizeof(sym.name) - 1);
        sym.value = as->symbols[i].value;
        sym.section = as->symbols[i].section;
        sym.type = as->symbols[i].type;
        sym.defined = as->symbols[i].defined;

        if (fwrite(&sym, sizeof(ObjSymbol), 1, f) != 1) {
            fprintf(stderr, "[object] Error escribiendo simbolo %d\n", i);
            fclose(f);
            return -1;
        }
    }

    for (int i = 0; i < as->num_fixups; i++) {
        ObjRelocation rel;
        memset(&rel, 0, sizeof(rel));
        strncpy(rel.symbol, as->fixups[i].symbol, sizeof(rel.symbol) - 1);
        rel.section = as->fixups[i].section;
        rel.offset = as->fixups[i].offset;
        rel.type = as->fixups[i].type;
        rel.addend = as->fixups[i].instr_end;

        if (fwrite(&rel, sizeof(ObjRelocation), 1, f) != 1) {
            fprintf(stderr, "[object] Error escribiendo relocacion %d\n", i);
            fclose(f);
            return -1;
        }
    }

    if (as->sections[SEC_TEXT].size > 0) {
        if (fwrite(as->sections[SEC_TEXT].data, 1, as->sections[SEC_TEXT].size, f) != (size_t)as->sections[SEC_TEXT].size) {
            fprintf(stderr, "[object] Error escribiendo datos de .text\n");
            fclose(f);
            return -1;
        }
    }

    if (as->sections[SEC_DATA].size > 0) {
        if (fwrite(as->sections[SEC_DATA].data, 1, as->sections[SEC_DATA].size, f) != (size_t)as->sections[SEC_DATA].size) {
            fprintf(stderr, "[object] Error escribiendo datos de .data\n");
            fclose(f);
            return -1;
        }
    }

    fclose(f);
    return 0;
}

int read_object_file(const char *filename, AssemblerState *as) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror("[object] fopen read failed");
        return -1;
    }

    ObjHeader header;
    if (fread(&header, sizeof(ObjHeader), 1, f) != 1) {
        fprintf(stderr, "[object] Error leyendo cabecera\n");
        fclose(f);
        return -1;
    }

    if (memcmp(header.magic, OBJ_MAGIC, strlen(OBJ_MAGIC)) != 0) {
        fprintf(stderr, "[object] Firma magic invalida: %.7s\n", header.magic);
        fclose(f);
        return -1;
    }

    ObjSectionHeader sec_hdrs[MAX_SECTIONS];
    if (fread(sec_hdrs, sizeof(ObjSectionHeader), MAX_SECTIONS, f) != MAX_SECTIONS) {
        fprintf(stderr, "[object] Error leyendo cabeceras de seccion\n");
        fclose(f);
        return -1;
    }

    assembler_init(as);

    for (int i = 0; i < MAX_SECTIONS; i++) {
        as->sections[i].type = sec_hdrs[i].type;
        as->sections[i].size = sec_hdrs[i].size;
        as->sections[i].base_addr = sec_hdrs[i].base_addr;
    }

    as->num_symbols = header.num_symbols;
    for (uint32_t i = 0; i < header.num_symbols; i++) {
        ObjSymbol sym;
        if (fread(&sym, sizeof(ObjSymbol), 1, f) != 1) {
            fprintf(stderr, "[object] Error leyendo simbolo %u\n", i);
            fclose(f);
            return -1;
        }
        strncpy(as->symbols[i].name, sym.name, sizeof(as->symbols[i].name) - 1);
        as->symbols[i].value = sym.value;
        as->symbols[i].section = (SectionType)sym.section;
        as->symbols[i].type = (SymbolType)sym.type;
        as->symbols[i].defined = sym.defined;
    }

    as->num_fixups = header.num_relocations;
    for (uint32_t i = 0; i < header.num_relocations; i++) {
        ObjRelocation rel;
        if (fread(&rel, sizeof(ObjRelocation), 1, f) != 1) {
            fprintf(stderr, "[object] Error leyendo relocacion %u\n", i);
            fclose(f);
            return -1;
        }
        strncpy(as->fixups[i].symbol, rel.symbol, sizeof(as->fixups[i].symbol) - 1);
        as->fixups[i].section = (SectionType)rel.section;
        as->fixups[i].offset = rel.offset;
        as->fixups[i].type = (FixupType)rel.type;
        as->fixups[i].instr_end = rel.addend;
    }

    if (as->sections[SEC_TEXT].size > 0) {
        fseek(f, sec_hdrs[SEC_TEXT].offset, SEEK_SET);
        if (fread(as->sections[SEC_TEXT].data, 1, as->sections[SEC_TEXT].size, f) != (size_t)as->sections[SEC_TEXT].size) {
            fprintf(stderr, "[object] Error leyendo datos de .text\n");
            fclose(f);
            return -1;
        }
    }

    if (as->sections[SEC_DATA].size > 0) {
        fseek(f, sec_hdrs[SEC_DATA].offset, SEEK_SET);
        if (fread(as->sections[SEC_DATA].data, 1, as->sections[SEC_DATA].size, f) != (size_t)as->sections[SEC_DATA].size) {
            fprintf(stderr, "[object] Error leyendo datos de .data\n");
            fclose(f);
            return -1;
        }
    }

    fclose(f);
    return 0;
}
