#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "assembler.h"
#include "object.h"

#define MAX_INPUT_FILES 32

typedef struct {
    char name[64];
    uint32_t value;           // Dirección absoluta final en memoria
    int defined;
    int object_file_index;
    SymbolType type;
} GlobalSymbol;

static GlobalSymbol global_symbols[MAX_SYMBOLS * MAX_INPUT_FILES];
static int num_global_symbols = 0;

static uint32_t read_u32_le(const uint8_t *ptr) {
    return ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
}

static void write_u32_le(uint8_t *ptr, uint32_t val) {
    ptr[0] = val & 0xFF;
    ptr[1] = (val >> 8) & 0xFF;
    ptr[2] = (val >> 16) & 0xFF;
    ptr[3] = (val >> 24) & 0xFF;
}

static GlobalSymbol *find_global_symbol(const char *name) {
    for (int i = 0; i < num_global_symbols; i++) {
        if (strcmp(global_symbols[i].name, name) == 0)
            return &global_symbols[i];
    }
    return NULL;
}

static int add_global_symbol(const char *name, uint32_t value, int defined, int obj_idx, SymbolType type) {
    GlobalSymbol *existing = find_global_symbol(name);
    if (existing) {
        if (defined && existing->defined) {
            fprintf(stderr, "[linker] error: simbolo '%s' duplicado (definido en objeto %d y objeto %d)\n",
                    name, existing->object_file_index, obj_idx);
            return -1;
        }
        if (defined) {
            existing->value = value;
            existing->defined = 1;
            existing->object_file_index = obj_idx;
            existing->type = type;
        }
        return 0;
    }

    if (num_global_symbols >= MAX_SYMBOLS * MAX_INPUT_FILES) {
        fprintf(stderr, "[linker] error: tabla global de simbolos llena\n");
        return -1;
    }

    GlobalSymbol *gs = &global_symbols[num_global_symbols++];
    strncpy(gs->name, name, sizeof(gs->name) - 1);
    gs->value = value;
    gs->defined = defined;
    gs->object_file_index = obj_idx;
    gs->type = type;
    return 0;
}

int main(int argc, char *argv[]) {
    printf("=== Mini Linker IA-32 ===\n");
    printf("Integrante 5: Formato Objeto y Linker\n\n");

    if (argc < 2) {
        fprintf(stderr, "Uso: %s [-o program.bin] [-b base_addr] obj1.obj [obj2.obj ...]\n", argv[0]);
        return 1;
    }

    const char *output_file = "program.bin";
    uint32_t base_addr = 0x00000000;
    const char *input_files[MAX_INPUT_FILES];
    int num_inputs = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                output_file = argv[++i];
            } else {
                fprintf(stderr, "Error: se esperaba un archivo de salida despues de -o\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-b") == 0) {
            if (i + 1 < argc) {
                base_addr = (uint32_t)strtoul(argv[++i], NULL, 0);
            } else {
                fprintf(stderr, "Error: se esperaba una direccion base despues de -b\n");
                return 1;
            }
        } else {
            if (num_inputs >= MAX_INPUT_FILES) {
                fprintf(stderr, "Error: demasiados archivos objeto de entrada (max %d)\n", MAX_INPUT_FILES);
                return 1;
            }
            input_files[num_inputs++] = argv[i];
        }
    }

    if (num_inputs == 0) {
        fprintf(stderr, "Error: no se especificaron archivos objeto de entrada\n");
        return 1;
    }

    // Cargar todos los archivos objeto
    static AssemblerState inputs[MAX_INPUT_FILES];
    for (int i = 0; i < num_inputs; i++) {
        printf("[linker] Cargando objeto '%s'...\n", input_files[i]);
        if (read_object_file(input_files[i], &inputs[i]) != 0) {
            fprintf(stderr, "[linker] Error leyendo '%s'\n", input_files[i]);
            return 1;
        }
    }

    // Calcular offsets de secciones para fusionado secuencial
    uint32_t text_offsets[MAX_INPUT_FILES];
    uint32_t data_offsets[MAX_INPUT_FILES];
    uint32_t bss_offsets[MAX_INPUT_FILES];

    uint32_t total_text_size = 0;
    uint32_t total_data_size = 0;
    uint32_t total_bss_size = 0;

    for (int i = 0; i < num_inputs; i++) {
        text_offsets[i] = total_text_size;
        total_text_size += inputs[i].sections[SEC_TEXT].size;

        data_offsets[i] = total_data_size;
        total_data_size += inputs[i].sections[SEC_DATA].size;

        bss_offsets[i] = total_bss_size;
        total_bss_size += inputs[i].sections[SEC_BSS].size;
    }

    // Calcular direcciones de inicio de las secciones fusionadas
    uint32_t text_base = base_addr;
    uint32_t data_base = text_base + total_text_size;
    uint32_t bss_base = data_base + total_data_size;

    printf("\n[linker] Direcciones base calculadas:\n");
    printf("  .text: 0x%08X (size %u bytes)\n", text_base, total_text_size);
    printf("  .data: 0x%08X (size %u bytes)\n", data_base, total_data_size);
    printf("  .bss : 0x%08X (size %u bytes)\n\n", bss_base, total_bss_size);

    // Registrar todos los símbolos globales y externos
    int linker_errors = 0;
    for (int i = 0; i < num_inputs; i++) {
        for (int j = 0; j < inputs[i].num_symbols; j++) {
            Symbol *s = &inputs[i].symbols[j];
            if (s->type == SYM_GLOBAL) {
                uint32_t abs_val = 0;
                if (s->defined) {
                    if (s->section == SEC_TEXT)
                        abs_val = text_base + text_offsets[i] + s->value;
                    else if (s->section == SEC_DATA)
                        abs_val = data_base + data_offsets[i] + s->value;
                    else if (s->section == SEC_BSS)
                        abs_val = bss_base + bss_offsets[i] + s->value;
                }
                if (add_global_symbol(s->name, abs_val, s->defined, i, SYM_GLOBAL) != 0) {
                    linker_errors++;
                }
            } else if (s->type == SYM_EXTERN) {
                if (add_global_symbol(s->name, 0, 0, i, SYM_EXTERN) != 0) {
                    linker_errors++;
                }
            }
        }
    }

    // Validar que todos los símbolos externos estén resueltos
    for (int i = 0; i < num_global_symbols; i++) {
        if (!global_symbols[i].defined) {
            fprintf(stderr, "[linker] error: simbolo externo '%s' no resuelto\n", global_symbols[i].name);
            linker_errors++;
        }
    }

    if (linker_errors > 0) {
        fprintf(stderr, "[linker] Enlace fallido con %d error(es)\n", linker_errors);
        return 1;
    }

    // Búferes para fusionar secciones
    uint8_t *merged_text = calloc(1, total_text_size > 0 ? total_text_size : 1);
    uint8_t *merged_data = calloc(1, total_data_size > 0 ? total_data_size : 1);

    // Copiar bytes de secciones a búferes fusionados
    for (int i = 0; i < num_inputs; i++) {
        if (inputs[i].sections[SEC_TEXT].size > 0) {
            memcpy(merged_text + text_offsets[i], inputs[i].sections[SEC_TEXT].data, inputs[i].sections[SEC_TEXT].size);
        }
        if (inputs[i].sections[SEC_DATA].size > 0) {
            memcpy(merged_data + data_offsets[i], inputs[i].sections[SEC_DATA].data, inputs[i].sections[SEC_DATA].size);
        }
    }

    // Aplicar relocaciones
    printf("[linker] Aplicando relocaciones...\n");
    for (int i = 0; i < num_inputs; i++) {
        for (int r = 0; r < inputs[i].num_fixups; r++) {
            Fixup *rel = &inputs[i].fixups[r];
            uint32_t sym_addr = 0;
            int found = 0;

            // 1. Buscar en la tabla de símbolos local del objeto i
            for (int s_idx = 0; s_idx < inputs[i].num_symbols; s_idx++) {
                Symbol *s = &inputs[i].symbols[s_idx];
                if (strcmp(s->name, rel->symbol) == 0 && s->defined && s->type != SYM_EXTERN) {
                    if (s->section == SEC_TEXT)
                        sym_addr = text_base + text_offsets[i] + s->value;
                    else if (s->section == SEC_DATA)
                        sym_addr = data_base + data_offsets[i] + s->value;
                    else if (s->section == SEC_BSS)
                        sym_addr = bss_base + bss_offsets[i] + s->value;
                    else if (s->type == SYM_EQU)
                        sym_addr = s->value;
                    found = 1;
                    break;
                }
            }

            // 2. Si no se encontró localmente, buscar en la tabla de símbolos global
            if (!found) {
                GlobalSymbol *gs = find_global_symbol(rel->symbol);
                if (gs && gs->defined) {
                    sym_addr = gs->value;
                    found = 1;
                }
            }

            if (!found) {
                fprintf(stderr, "[linker] error en objeto %s: no se pudo resolver la relocacion para el simbolo '%s'\n",
                        input_files[i], rel->symbol);
                linker_errors++;
                continue;
            }

            // Aplicar el parche
            uint8_t *patch_ptr = NULL;

            if (rel->section == SEC_TEXT) {
                patch_ptr = merged_text + text_offsets[i] + rel->offset;
            } else if (rel->section == SEC_DATA) {
                patch_ptr = merged_data + data_offsets[i] + rel->offset;
            } else {
                fprintf(stderr, "[linker] error: relocacion en seccion invalida %d\n", rel->section);
                linker_errors++;
                continue;
            }

            uint32_t existing_val = read_u32_le(patch_ptr);
            uint32_t val_to_write = 0;

            if (rel->type == FIX_ABS32) {
                val_to_write = sym_addr + existing_val;
                printf("  [ABS32] Objeto %d: Parcheado '%s' en offset %s:0x%04X con valor absoluto 0x%08X\n",
                       i, rel->symbol, rel->section == SEC_TEXT ? ".text" : ".data",
                       text_offsets[i] + rel->offset, val_to_write);
            } else if (rel->type == FIX_REL32) {
                // Dirección de fin de instrucción: instr_end_addr
                uint32_t instr_end_addr = text_base + text_offsets[i] + rel->instr_end;
                val_to_write = sym_addr - instr_end_addr + existing_val;
                printf("  [REL32] Objeto %d: Parcheado '%s' en offset %s:0x%04X con valor relativo 0x%08X (PC=0x%08X)\n",
                       i, rel->symbol, rel->section == SEC_TEXT ? ".text" : ".data",
                       text_offsets[i] + rel->offset, val_to_write, instr_end_addr);
            }

            write_u32_le(patch_ptr, val_to_write);
        }
    }

    if (linker_errors > 0) {
        fprintf(stderr, "[linker] Enlace fallido durante la aplicacion de relocaciones\n");
        free(merged_text);
        free(merged_data);
        return 1;
    }

    // Escribir el ejecutable binario final
    FILE *out = fopen(output_file, "wb");
    if (!out) {
        perror("[linker] Error abriendo archivo de salida");
        free(merged_text);
        free(merged_data);
        return 1;
    }

    if (total_text_size > 0) {
        fwrite(merged_text, 1, total_text_size, out);
    }
    if (total_data_size > 0) {
        fwrite(merged_data, 1, total_data_size, out);
    }
    fclose(out);

    printf("\n[linker] Enlace exitoso. Binario final escrito a '%s' (%u bytes)\n",
           output_file, total_text_size + total_data_size);

    // Escribir archivo de mapa (.map) para depuración
    char map_file[256];
    snprintf(map_file, sizeof(map_file), "%s.map", output_file);
    FILE *map = fopen(map_file, "w");
    if (map) {
        fprintf(map, "=== MAPA DE ENLACE IA-32 ===\n\n");
        fprintf(map, "Direccion base: 0x%08X\n\n", base_addr);
        fprintf(map, "SECCIONES FUSIONADAS:\n");
        fprintf(map, "  %-8s  %-10s  %-10s\n", "Nombre", "Dir Inicio", "Tamaño");
        fprintf(map, "  -------------------------------------\n");
        fprintf(map, "  %-8s  0x%08X  %u bytes\n", ".text", text_base, total_text_size);
        fprintf(map, "  %-8s  0x%08X  %u bytes\n", ".data", data_base, total_data_size);
        fprintf(map, "  %-8s  0x%08X  %u bytes\n\n", ".bss", bss_base, total_bss_size);

        fprintf(map, "DISTRIBUCION DE ARCHIVOS EN SECCIONES:\n");
        for (int i = 0; i < num_inputs; i++) {
            fprintf(map, "  Objeto [%d] '%s':\n", i, input_files[i]);
            fprintf(map, "    .text offset: 0x%08X, size: %u bytes\n", text_offsets[i], inputs[i].sections[SEC_TEXT].size);
            fprintf(map, "    .data offset: 0x%08X, size: %u bytes\n", data_offsets[i], inputs[i].sections[SEC_DATA].size);
            fprintf(map, "    .bss  offset: 0x%08X, size: %u bytes\n", bss_offsets[i], inputs[i].sections[SEC_BSS].size);
        }
        fprintf(map, "\nTABLA GLOBAL DE SIMBOLOS:\n");
        fprintf(map, "  %-20s  %-10s  %-8s  %s\n", "Nombre", "Direccion", "Definido", "Origen");
        fprintf(map, "  ---------------------------------------------------------------\n");
        for (int i = 0; i < num_global_symbols; i++) {
            fprintf(map, "  %-20s  0x%08X  %-8s  Objeto [%d] '%s'\n",
                    global_symbols[i].name,
                    global_symbols[i].value,
                    global_symbols[i].defined ? "si" : "no",
                    global_symbols[i].object_file_index,
                    input_files[global_symbols[i].object_file_index]);
        }

        fprintf(map, "\nTABLAS LOCALES DE SIMBOLOS (LOCAL / EQU):\n");
        for (int i = 0; i < num_inputs; i++) {
            fprintf(map, "  Objeto [%d] '%s':\n", i, input_files[i]);
            for (int j = 0; j < inputs[i].num_symbols; j++) {
                Symbol *s = &inputs[i].symbols[j];
                if (s->type == SYM_LOCAL || s->type == SYM_EQU) {
                    uint32_t abs_addr = s->value;
                    if (s->type == SYM_LOCAL) {
                        if (s->section == SEC_TEXT) abs_addr += text_base + text_offsets[i];
                        else if (s->section == SEC_DATA) abs_addr += data_base + data_offsets[i];
                        else if (s->section == SEC_BSS) abs_addr += bss_base + bss_offsets[i];
                    }
                    fprintf(map, "    %-20s  0x%08X  %s\n",
                            s->name, abs_addr, s->type == SYM_LOCAL ? "LOCAL" : "EQU");
                }
            }
        }
        fclose(map);
        printf("[linker] Archivo mapa de enlace escrito a '%s'\n", map_file);
    }

    free(merged_text);
    free(merged_data);
    return 0;
}
