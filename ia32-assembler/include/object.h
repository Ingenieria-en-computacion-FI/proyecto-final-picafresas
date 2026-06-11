#ifndef OBJECT_H
#define OBJECT_H

#include <stdint.h>
#include "assembler.h"

#define OBJ_MAGIC "IA32OBJ"

// Estructura del Encabezado del archivo objeto
typedef struct {
    char magic[8];           // "IA32OBJ\0"
    uint32_t num_sections;   // Número de secciones guardadas (típicamente 3)
    uint32_t num_symbols;    // Número de símbolos en la tabla
    uint32_t num_relocations;// Número de relocaciones
} ObjHeader;

// Estructura de Sección en el archivo objeto
typedef struct {
    char name[16];           // ".text", ".data", ".bss"
    uint32_t type;           // SectionType (0, 1, 2)
    uint32_t size;           // Tamaño de la sección en bytes
    uint32_t offset;         // Offset al contenido de la sección en el archivo
    uint32_t base_addr;      // Dirección base (ORG)
} ObjSectionHeader;

// Estructura de Símbolo en el archivo objeto
typedef struct {
    char name[64];           // Nombre del símbolo
    uint32_t value;          // Valor/Offset del símbolo
    uint32_t section;        // SectionType (0, 1, 2) o -1 (extern/absoluto)
    uint32_t type;           // SymbolType (SYM_LOCAL, SYM_GLOBAL, SYM_EXTERN, SYM_EQU)
    uint32_t defined;        // 1 si está definido, 0 si no
} ObjSymbol;

// Estructura de Relocación en el archivo objeto
typedef struct {
    char symbol[64];         // Símbolo asociado
    uint32_t section;        // Sección donde se aplica (0: text, 1: data)
    uint32_t offset;         // Offset dentro de la sección donde parchear los 4 bytes
    uint32_t type;           // FixupType (FIX_REL32, FIX_ABS32)
    int32_t addend;          // Addend para el cálculo del valor a escribir
} ObjRelocation;

// Escribe el archivo objeto binario a partir del AssemblerState
int write_object_file(const char *filename, const AssemblerState *as);

// Lee el archivo objeto binario y llena el AssemblerState
int read_object_file(const char *filename, AssemblerState *as);

#endif
