#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "assembler.h"
#include "object.h"

void dump_object_file(const char *filename) {
    printf("========================================================\n");
    printf("ARCHIVO OBJETO: %s\n", filename);
    printf("========================================================\n");

    AssemblerState as;
    if (read_object_file(filename, &as) != 0) {
        fprintf(stderr, "Error: no se pudo leer el archivo objeto '%s'\n", filename);
        return;
    }

    const char *sec_names[] = {".text", ".data", ".bss"};
    const char *sym_types[] = {"LOCAL", "GLOBAL", "EXTERN", "EQU"};
    const char *rel_types[] = {"FIX_REL32", "FIX_ABS32"};

    printf("CABECERA:\n");
    printf("  Magic:      IA32OBJ\n");
    printf("  Secciones:  %d\n", MAX_SECTIONS);
    printf("  Simbolos:   %d\n", as.num_symbols);
    printf("  Relocs:     %d\n\n", as.num_fixups);

    printf("SECCIONES:\n");
    printf("  Idx  %-8s  %-10s  %-10s\n", "Nombre", "Tamano", "Dir Base");
    printf("  -------------------------------------\n");
    for (int i = 0; i < MAX_SECTIONS; i++) {
        printf("  %3d  %-8s  %u bytes  0x%08X\n",
               i, as.sections[i].name, as.sections[i].size, as.sections[i].base_addr);
    }
    printf("\n");

    printf("TABLA DE SIMBOLOS (%d entradas):\n", as.num_symbols);
    printf("  %-20s  %-8s  %-8s  %-7s  %s\n",
           "Nombre", "Valor", "seccion", "Tipo", "Definido");
    printf("  ------------------------------------------------------\n");
    for (int i = 0; i < as.num_symbols; i++) {
        Symbol *s = &as.symbols[i];
        const char *sec_name = "ABS/UND";
        if (s->section >= 0 && s->section < MAX_SECTIONS) {
            sec_name = sec_names[s->section];
        }
        printf("  %-20s  %08X  %-8s  %-7s  %s\n",
               s->name, s->value, sec_name, sym_types[s->type], s->defined ? "si" : "no");
    }
    printf("\n");

    printf("TABLA DE RELOCACIONES (%d entradas):\n", as.num_fixups);
    printf("  %-20s  %-8s  %-8s  %-10s  %s\n",
           "Simbolo", "seccion", "Offset", "Tipo", "Fin Instr / Addend");
    printf("  ---------------------------------------------------------------\n");
    for (int i = 0; i < as.num_fixups; i++) {
        Fixup *f = &as.fixups[i];
        const char *sec_name = "desconocido";
        if (f->section >= 0 && f->section < MAX_SECTIONS) {
            sec_name = sec_names[f->section];
        }
        printf("  %-20s  %-8s  0x%04X    %-10s  0x%04X (%d)\n",
               f->symbol, sec_name, f->offset, rel_types[f->type], f->instr_end, f->instr_end);
    }
    printf("\n");

    for (int s = 0; s < MAX_SECTIONS; s++) {
        if (s == SEC_BSS) continue;
        Section *sec = &as.sections[s];
        if (sec->size == 0) continue;

        printf("HEXDUMP seccion %s (%d bytes):\n", sec->name, sec->size);
        for (int i = 0; i < sec->size; i++) {
            if (i % 16 == 0) {
                printf("  %04X: ", i);
            }
            printf("%02X ", sec->data[i]);
            if (i % 16 == 15) {
                printf("\n");
            }
        }
        if (sec->size % 16 != 0) {
            printf("\n");
        }
        printf("\n");
    }
    printf("========================================================\n\n");
}

int main(int argc, char *argv[]) {
    printf("=== Herramienta Objdump IA-32 ===\n");
    printf("Integrante 5: Formato Objeto y Linker\n\n");

    if (argc < 2) {
        fprintf(stderr, "Uso: %s archivo1.obj [archivo2.obj ...]\n", argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        dump_object_file(argv[i]);
    }

    return 0;
}
