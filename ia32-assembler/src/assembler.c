#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "assembler.h"
#include <ctype.h>

/* integrante 4*/
#ifndef ENCODER_H
typedef struct {
    uint8_t bytes[15];
    int len;
    int fixup_offset;
    int is_relative;
} EncodedInstr;

typedef struct {
    uint8_t modrm;
    uint8_t sib;
    int has_sib;
    int disp_size;
    int32_t disp;
    int label_fixup;
    int valid;
} ModRMResult;

static uint8_t reg_code(Register reg) {
    return (uint8_t)reg;
}

/*  ModR/M para operandos de registro */
static int is_disp8(int32_t value) {
    return value >= -128 && value <= 127;
}

static int encode_reg_operand(Register reg, uint8_t reg_field,
                              ModRMResult *result) {
    result->modrm = (3 << 6) | (reg_field << 3) | reg_code(reg);
    result->has_sib = 0;
    result->disp_size = 0;
    result->disp = 0;
    result->valid = 1;
    return 1;
}

static int encode_memory_operand(const Operand *op, uint8_t reg_field,
                                 ModRMResult *result) {
    uint8_t mod = 0;
    uint8_t rm = 0;
    uint8_t sib = 0;
    int has_sib = 0;
    int disp_size = 0;
    int32_t disp = 0;
    int label_fixup = 0;

    if (!op || op->mode == ADDR_REGISTER || op->mode == ADDR_IMMEDIATE)
        return 0;

    switch (op->mode) {
    case ADDR_DIRECT:
        mod = 0;
        rm = 5;
        disp_size = 4;
        disp = (int32_t)op->displacement;
        break;

    case ADDR_LABEL:
        mod = 0;
        rm = 5;
        disp_size = 4;
        disp = 0;
        label_fixup = 1;
        break;

    case ADDR_BASE_DISP:
        if (op->reg == REG_ESP) {
            has_sib = 1;
            rm = 4;
            if (op->displacement == 0) {
                mod = 0;
                disp_size = 0;
            } else if (is_disp8((int32_t)op->displacement)) {
                mod = 1;
                disp_size = 1;
                disp = (int32_t)op->displacement;
            } else {
                mod = 2;
                disp_size = 4;
                disp = (int32_t)op->displacement;
            }
            sib = (0 << 6) | (4 << 3) | reg_code(op->reg);
        } else if (op->reg == REG_EBP && op->displacement == 0) {
            mod = 1;
            rm = 5;
            disp_size = 1;
            disp = 0;
        } else {
            if (op->displacement == 0) {
                mod = 0;
                rm = reg_code(op->reg);
            } else if (is_disp8((int32_t)op->displacement)) {
                mod = 1;
                rm = reg_code(op->reg);
                disp_size = 1;
                disp = (int32_t)op->displacement;
            } else {
                mod = 2;
                rm = reg_code(op->reg);
                disp_size = 4;
                disp = (int32_t)op->displacement;
            }
        }
        break;

    case ADDR_BASE_INDEX:
        if (op->index == REG_ESP)
            return 0;

        has_sib = 1;
        rm = 4;
        if (op->displacement == 0) {
            if (op->reg == REG_EBP) {
                mod = 1;
                disp_size = 1;
                disp = 0;
            } else {
                mod = 0;
            }
        } else if (is_disp8((int32_t)op->displacement)) {
            mod = 1;
            disp_size = 1;
            disp = (int32_t)op->displacement;
        } else {
            mod = 2;
            disp_size = 4;
            disp = (int32_t)op->displacement;
        }
        sib = (0 << 6) | (reg_code(op->index) << 3) | reg_code(op->reg);
        break;

    case ADDR_BASE_INDEX_SCALED:
    case ADDR_SIB_DISP: {
        if (op->index == REG_ESP)
            return 0;

        int scale;
        switch (op->scale) {
        case 1: scale = 0; break;
        case 2: scale = 1; break;
        case 4: scale = 2; break;
        case 8: scale = 3; break;
        default:
            return 0;
        }

        has_sib = 1;
        rm = 4;

        if (op->reg == REG_NONE) {
            if (op->displacement == 0) {
                mod = 0;
                disp_size = 4;
            } else if (is_disp8((int32_t)op->displacement)) {
                mod = 1;
                disp_size = 1;
                disp = (int32_t)op->displacement;
            } else {
                mod = 2;
                disp_size = 4;
                disp = (int32_t)op->displacement;
            }
            sib = (scale << 6) | (reg_code(op->index) << 3) | 5;
            break;
        }

        if (op->displacement == 0) {
            if (op->reg == REG_EBP) {
                mod = 1;
                disp_size = 1;
                disp = 0;
            } else {
                mod = 0;
            }
        } else if (is_disp8((int32_t)op->displacement)) {
            mod = 1;
            disp_size = 1;
            disp = (int32_t)op->displacement;
        } else {
            mod = 2;
            disp_size = 4;
            disp = (int32_t)op->displacement;
        }

        sib = (scale << 6) | (reg_code(op->index) << 3) | reg_code(op->reg);
        break;
    }

    default:
        return 0;
    }

    result->modrm = (mod << 6) | (reg_field << 3) | rm;
    result->sib = sib;
    result->has_sib = has_sib;
    result->disp_size = disp_size;
    result->disp = disp;
    result->label_fixup = label_fixup;
    result->valid = 1;
    return 1;
}

static void append_int32(EncodedInstr *enc, int32_t value) {
    /* immediate y generación final de bytes */
    if (enc->len + 4 > (int)sizeof(enc->bytes))
        return;

    enc->bytes[enc->len++] = (uint8_t)(value & 0xFF);
    enc->bytes[enc->len++] = (uint8_t)((value >> 8) & 0xFF);
    enc->bytes[enc->len++] = (uint8_t)((value >> 16) & 0xFF);
    enc->bytes[enc->len++] = (uint8_t)((value >> 24) & 0xFF);
}

static void append_int8(EncodedInstr *enc, int8_t value) {
    if (enc->len + 1 > (int)sizeof(enc->bytes))
        return;

    enc->bytes[enc->len++] = (uint8_t)value;
}

static void append_disp_bytes(EncodedInstr *enc, int32_t disp, int size) {
    if (size == 1) {
        append_int8(enc, (int8_t)disp);
    } else if (size == 4) {
        append_int32(enc, disp);
    }
}

/* generacion final de bytes*/
static void append_modrm_sib(EncodedInstr *enc, const ModRMResult *m) {
    if (enc->len + 1 > (int)sizeof(enc->bytes))
        return;

    enc->bytes[enc->len++] = m->modrm;

    if (m->has_sib) {
        if (enc->len + 1 > (int)sizeof(enc->bytes))
            return;
        enc->bytes[enc->len++] = m->sib;
    }

    append_disp_bytes(enc, m->disp, m->disp_size);
}

static EncodedInstr encode_instruction(const ASTNode *node) {
    EncodedInstr enc;
    memset(&enc, 0, sizeof(enc));
    enc.fixup_offset = -1;

    if (!node)
        return enc;

    const Operand *op0 = node->num_operands > 0 ? &node->operands[0] : NULL;
    const Operand *op1 = node->num_operands > 1 ? &node->operands[1] : NULL;
    ModRMResult modrm = {0};

    switch (node->opcode) {
    case OP_NOP:
        enc.bytes[enc.len++] = 0x90;
        break;

    case OP_RET:
        enc.bytes[enc.len++] = 0xC3;
        break;

    case OP_INT:
        if (!op0 || op0->mode != ADDR_IMMEDIATE)
            break;

        enc.bytes[enc.len++] = 0xCD;
        append_int8(&enc, (int8_t)op0->immediate);
        break;

    case OP_PUSH:
        if (!op0)
            break;

        if (op0->mode == ADDR_REGISTER) {
            enc.bytes[enc.len++] = 0x50 + reg_code(op0->reg);
        } else if (op0->mode == ADDR_IMMEDIATE || op0->mode == ADDR_LABEL) {
            enc.bytes[enc.len++] = 0x68;
            enc.fixup_offset = enc.len;
            enc.is_relative = 0;
            append_int32(&enc, op0->mode == ADDR_IMMEDIATE ?
                         (int32_t)op0->immediate : 0);
        } else {
            if (!encode_memory_operand(op0, 6, &modrm))
                break;
            enc.bytes[enc.len++] = 0xFF;
            append_modrm_sib(&enc, &modrm);
        }
        break;

    case OP_POP:
        if (!op0)
            break;

        if (op0->mode == ADDR_REGISTER) {
            enc.bytes[enc.len++] = 0x58 + reg_code(op0->reg);
        } else {
            if (!encode_memory_operand(op0, 0, &modrm))
                break;
            enc.bytes[enc.len++] = 0x8F;
            append_modrm_sib(&enc, &modrm);
        }
        break;

    case OP_LEA:
        if (!op0 || !op1 || op0->mode != ADDR_REGISTER)
            break;
        if (!encode_memory_operand(op1, reg_code(op0->reg), &modrm))
            break;
        enc.bytes[enc.len++] = 0x8D;
        append_modrm_sib(&enc, &modrm);
        break;

    case OP_MOV:
        if (!op0 || !op1)
            break;

        if (op0->mode == ADDR_REGISTER) {
            if (op1->mode == ADDR_REGISTER || op1->mode == ADDR_DIRECT ||
                op1->mode == ADDR_BASE_DISP || op1->mode == ADDR_BASE_INDEX ||
                op1->mode == ADDR_BASE_INDEX_SCALED || op1->mode == ADDR_SIB_DISP) {
                enc.bytes[enc.len++] = 0x8B;
                if (!encode_memory_operand(op1, reg_code(op0->reg), &modrm))
                    break;
                append_modrm_sib(&enc, &modrm);
            } else if (op1->mode == ADDR_IMMEDIATE || op1->mode == ADDR_LABEL) {
                enc.bytes[enc.len++] = 0xB8 + reg_code(op0->reg);
                enc.fixup_offset = enc.len;
                enc.is_relative = 0;
                append_int32(&enc, op1->mode == ADDR_IMMEDIATE ?
                             (int32_t)op1->immediate : 0);
            } else {
                break;
            }
        } else if (op0->mode == ADDR_DIRECT || op0->mode == ADDR_BASE_DISP ||
                   op0->mode == ADDR_BASE_INDEX || op0->mode == ADDR_BASE_INDEX_SCALED ||
                   op0->mode == ADDR_SIB_DISP) {
            if (op1->mode != ADDR_REGISTER && op1->mode != ADDR_IMMEDIATE &&
                op1->mode != ADDR_LABEL)
                break;

            if (op1->mode == ADDR_REGISTER) {
                enc.bytes[enc.len++] = 0x89;
                if (!encode_memory_operand(op0, reg_code(op1->reg), &modrm))
                    break;
                append_modrm_sib(&enc, &modrm);
            } else {
                enc.bytes[enc.len++] = 0xC7;
                if (!encode_memory_operand(op0, 0, &modrm))
                    break;
                append_modrm_sib(&enc, &modrm);
                enc.fixup_offset = enc.len;
                enc.is_relative = 0;
                append_int32(&enc, op1->mode == ADDR_IMMEDIATE ?
                             (int32_t)op1->immediate : 0);
            }
        }
        break;

    case OP_ADD:
    case OP_SUB:
    case OP_CMP:
    case OP_AND:
    case OP_OR:
    case OP_XOR: {
        if (!op0 || !op1)
            break;

        uint8_t opcode_rm = 0;
        uint8_t opcode_r = 0;
        uint8_t opcode_imm = 0;
        uint8_t op_mod = 0;

        switch (node->opcode) {
        case OP_ADD: opcode_rm = 0x01; opcode_r = 0x03; opcode_imm = 0x81; op_mod = 0; break;
        case OP_SUB: opcode_rm = 0x29; opcode_r = 0x2B; opcode_imm = 0x81; op_mod = 5; break;
        case OP_CMP: opcode_rm = 0x39; opcode_r = 0x3B; opcode_imm = 0x81; op_mod = 7; break;
        case OP_AND: opcode_rm = 0x21; opcode_r = 0x23; opcode_imm = 0x81; op_mod = 4; break;
        case OP_OR:  opcode_rm = 0x09; opcode_r = 0x0B; opcode_imm = 0x81; op_mod = 1; break;
        case OP_XOR: opcode_rm = 0x31; opcode_r = 0x33; opcode_imm = 0x81; op_mod = 6; break;
        default:
            break;
        }

        if (op1->mode == ADDR_REGISTER || op1->mode == ADDR_DIRECT ||
            op1->mode == ADDR_BASE_DISP || op1->mode == ADDR_BASE_INDEX ||
            op1->mode == ADDR_BASE_INDEX_SCALED || op1->mode == ADDR_SIB_DISP) {
            if (op0->mode == ADDR_REGISTER) {
                enc.bytes[enc.len++] = opcode_r;
                if (op1->mode == ADDR_REGISTER) {
                    if (!encode_reg_operand(op1->reg, reg_code(op0->reg), &modrm))
                        break;
                } else {
                    if (!encode_memory_operand(op1, reg_code(op0->reg), &modrm))
                        break;
                }
                append_modrm_sib(&enc, &modrm);
            } else if (op0->mode == ADDR_DIRECT || op0->mode == ADDR_BASE_DISP ||
                       op0->mode == ADDR_BASE_INDEX || op0->mode == ADDR_BASE_INDEX_SCALED ||
                       op0->mode == ADDR_SIB_DISP) {
                if (op1->mode != ADDR_REGISTER)
                    break;
                enc.bytes[enc.len++] = opcode_rm;
                if (!encode_memory_operand(op0, reg_code(op1->reg), &modrm))
                    break;
                append_modrm_sib(&enc, &modrm);
            } else {
                break;
            }
        } else if (op1->mode == ADDR_IMMEDIATE || op1->mode == ADDR_LABEL) {
            if (op0->mode == ADDR_REGISTER) {
                enc.bytes[enc.len++] = opcode_imm;
                if (!encode_reg_operand(op0->reg, op_mod, &modrm))
                    break;
                append_modrm_sib(&enc, &modrm);
                enc.fixup_offset = enc.len;
                enc.is_relative = 0;
                append_int32(&enc, op1->mode == ADDR_IMMEDIATE ?
                             (int32_t)op1->immediate : 0);
            } else if (op0->mode == ADDR_DIRECT || op0->mode == ADDR_BASE_DISP ||
                       op0->mode == ADDR_BASE_INDEX || op0->mode == ADDR_BASE_INDEX_SCALED ||
                       op0->mode == ADDR_SIB_DISP) {
                enc.bytes[enc.len++] = opcode_imm;
                if (!encode_memory_operand(op0, op_mod, &modrm))
                    break;
                append_modrm_sib(&enc, &modrm);
                enc.fixup_offset = enc.len;
                enc.is_relative = 0;
                append_int32(&enc, op1->mode == ADDR_IMMEDIATE ?
                             (int32_t)op1->immediate : 0);
            }
        }
        break;
    }

    case OP_INC:
    case OP_DEC:
        if (!op0)
            break;

        if (op0->mode == ADDR_REGISTER) {
            enc.bytes[enc.len++] = (node->opcode == OP_INC ? 0x40 : 0x48) +
                                   reg_code(op0->reg);
        } else {
            if (!encode_memory_operand(op0, node->opcode == OP_INC ? 0 : 1, &modrm))
                break;
            enc.bytes[enc.len++] = 0xFF;
            append_modrm_sib(&enc, &modrm);
        }
        break;

    case OP_NEG:
    case OP_NOT:
        if (!op0)
            break;

        if (op0->mode == ADDR_REGISTER) {
            if (!encode_reg_operand(op0->reg, node->opcode == OP_NEG ? 3 : 2,
                                     &modrm))
                break;
            enc.bytes[enc.len++] = 0xF7;
            append_modrm_sib(&enc, &modrm);
        } else {
            if (!encode_memory_operand(op0, node->opcode == OP_NEG ? 3 : 2,
                                       &modrm))
                break;
            enc.bytes[enc.len++] = 0xF7;
            append_modrm_sib(&enc, &modrm);
        }
        break;

    case OP_MUL:
    case OP_DIV:
    case OP_IMUL:
    case OP_IDIV:
        if (!op0)
            break;

        int reg_field = 4;
        if (node->opcode == OP_DIV) reg_field = 6;
        else if (node->opcode == OP_IMUL) reg_field = 5;
        else if (node->opcode == OP_IDIV) reg_field = 7;

        if (op0->mode == ADDR_REGISTER) {
            if (!encode_reg_operand(op0->reg, reg_field, &modrm))
                break;
            enc.bytes[enc.len++] = 0xF7;
            append_modrm_sib(&enc, &modrm);
        } else {
            if (!encode_memory_operand(op0, reg_field, &modrm))
                break;
            enc.bytes[enc.len++] = 0xF7;
            append_modrm_sib(&enc, &modrm);
        }
        break;

    case OP_CALL:
        if (!op0)
            break;

        enc.bytes[enc.len++] = 0xE8;
        if (op0->mode == ADDR_LABEL) {
            enc.fixup_offset = enc.len;
            enc.is_relative = 1;
            append_int32(&enc, 0);
        } else if (op0->mode == ADDR_IMMEDIATE) {
            append_int32(&enc, (int32_t)op0->immediate);
        } else {
            break;
        }
        break;

    case OP_JMP:
        if (!op0)
            break;

        enc.bytes[enc.len++] = 0xE9;
        if (op0->mode == ADDR_LABEL) {
            enc.fixup_offset = enc.len;
            enc.is_relative = 1;
            append_int32(&enc, 0);
        } else if (op0->mode == ADDR_IMMEDIATE) {
            append_int32(&enc, (int32_t)op0->immediate);
        } else {
            break;
        }
        break;

    case OP_JE:
    case OP_JNE:
    case OP_JG:
    case OP_JL:
    case OP_JGE:
    case OP_JLE: {
        static const uint8_t cond_ops[] = {0x84, 0x85, 0x8F, 0x8C, 0x8D, 0x8E};
        int idx = -1;

        switch (node->opcode) {
        case OP_JE: idx = 0; break;
        case OP_JNE: idx = 1; break;
        case OP_JG: idx = 2; break;
        case OP_JL: idx = 3; break;
        case OP_JGE: idx = 4; break;
        case OP_JLE: idx = 5; break;
        default: break;
        }

        if (idx < 0)
            break;

        enc.bytes[enc.len++] = 0x0F;
        enc.bytes[enc.len++] = cond_ops[idx];
        if (op0->mode == ADDR_LABEL) {
            enc.fixup_offset = enc.len;
            enc.is_relative = 1;
            append_int32(&enc, 0);
        } else if (op0->mode == ADDR_IMMEDIATE) {
            append_int32(&enc, (int32_t)op0->immediate);
        } else {
            break;
        }
        break;
    }

    default:
        break;
    }

    if (enc.len > 0 && enc.fixup_offset >= enc.len) {
        enc.fixup_offset = -1;
    }

    return enc;
}

/* INTEGRANTE 4 */
#endif

void assembler_init(AssemblerState *as) {
    memset(as, 0, sizeof(AssemblerState));

    as->sections[SEC_TEXT].type = SEC_TEXT;
    as->sections[SEC_TEXT].name = ".text";
    as->sections[SEC_TEXT].base_addr = 0;

    as->sections[SEC_DATA].type = SEC_DATA;
    as->sections[SEC_DATA].name = ".data";
    as->sections[SEC_DATA].base_addr = 0;

    as->sections[SEC_BSS].type = SEC_BSS;
    as->sections[SEC_BSS].name = ".bss";
    as->sections[SEC_BSS].base_addr = 0;

    as->current_section = SEC_TEXT;
}

int current_offset(const AssemblerState *as) {
    return as->sections[as->current_section].size;
}

void emit_byte(AssemblerState *as, uint8_t byte) {
    Section *sec = &as->sections[as->current_section];

    if (sec->size >= MAX_SECTION_SIZE) {
        fprintf(stderr, "[assembler] seccion %s llena\n", sec->name);
        as->errors++;
        return;
    }

    sec->data[sec->size++] = byte;
}

void emit_bytes(AssemblerState *as, const uint8_t *data, int n) {
    for (int i = 0; i < n; i++)
        emit_byte(as, data[i]);
}

void emit_u32(AssemblerState *as, uint32_t val) {
    emit_byte(as, (uint8_t)(val & 0xFF));
    emit_byte(as, (uint8_t)((val >> 8) & 0xFF));
    emit_byte(as, (uint8_t)((val >> 16) & 0xFF));
    emit_byte(as, (uint8_t)((val >> 24) & 0xFF));
}

static void patch_u32(AssemblerState *as, SectionType sec, int offset, uint32_t val) {
    Section *s = &as->sections[sec];

    if (offset + 3 >= s->size) {
        fprintf(stderr, "[assembler] patch fuera de rango\n");
        as->errors++;
        return;
    }

    s->data[offset] = (uint8_t)(val & 0xFF);
    s->data[offset + 1] = (uint8_t)((val >> 8) & 0xFF);
    s->data[offset + 2] = (uint8_t)((val >> 16) & 0xFF);
    s->data[offset + 3] = (uint8_t)((val >> 24) & 0xFF);
}

Symbol *symbol_lookup(AssemblerState *as, const char *name) {
    for (int i = 0; i < as->num_symbols; i++) {
        if (strcmp(as->symbols[i].name, name) == 0)
            return &as->symbols[i];
    }

    return NULL;
}

int symbol_define(AssemblerState *as, const char *name,
                  uint32_t value, SectionType sec, SymbolType type) {
    Symbol *existing = symbol_lookup(as, name);

    if (existing) {
        if (existing->defined &&
            existing->type != SYM_GLOBAL &&
            existing->type != SYM_EXTERN) {
            fprintf(stderr, "[assembler] simbolo redefinido: '%s'\n", name);
            as->errors++;
            return -1;
        }

        if (type == SYM_GLOBAL) {
            existing->type = SYM_GLOBAL;
            return 0;
        }
        if (type == SYM_EXTERN) {
            existing->type = SYM_EXTERN;
            return 0;
        }

        existing->value = value;
        existing->section = sec;
        existing->type = type;
        existing->defined = 1;

        return 0;
    }

    if (as->num_symbols >= MAX_SYMBOLS) {
        fprintf(stderr, "[assembler] tabla de simbolos llena\n");
        as->errors++;
        return -1;
    }

    Symbol *s = &as->symbols[as->num_symbols++];
    strncpy(s->name, name, sizeof(s->name) - 1);
    s->value = value;
    s->section = sec;
    s->type = type;
    s->defined = (type != SYM_EXTERN && type != SYM_GLOBAL);

    return 0;
}

static int add_fixup(AssemblerState *as, const char *symbol,
                     FixupType type, int offset, int instr_end) {
    if (as->num_fixups >= MAX_FIXUPS) {
        fprintf(stderr, "[assembler] tabla de fixups llena\n");
        as->errors++;
        return -1;
    }

    Fixup *f = &as->fixups[as->num_fixups++];
    strncpy(f->symbol, symbol, sizeof(f->symbol) - 1);
    f->type = type;
    f->section = as->current_section;
    f->offset = offset;
    f->instr_end = instr_end;

    return 0;
}

static void resolve_fixups(AssemblerState *as) {
    for (int i = 0; i < as->num_fixups; i++) {
        Fixup *f = &as->fixups[i];
        Symbol *s = symbol_lookup(as, f->symbol);

        if (!s) {
            fprintf(stderr, "[assembler] simbolo no resuelto: '%s'\n", f->symbol);
            as->errors++;
            continue;
        }

        if (!s->defined) {
            if (s->type == SYM_EXTERN || s->type == SYM_GLOBAL) {
                continue;
            } else {
                fprintf(stderr, "[assembler] simbolo local no definido: '%s'\n", f->symbol);
                as->errors++;
                continue;
            }
        }

        uint32_t patch_val;

        if (f->type == FIX_REL32)
            patch_val = (uint32_t)(s->value - (uint32_t)f->instr_end);
        else
            patch_val = s->value;

        patch_u32(as, f->section, f->offset, patch_val);
    }
}

static void handle_section(AssemblerState *as, const ASTNode *node) {
    if (node->num_operands == 0)
        return;

    const char *name = node->operands[0].label;

    if (!name)
        return;

    char upper[32];
    int i;

    for (i = 0; name[i] && i < 31; i++)
        upper[i] = (char)toupper((unsigned char)name[i]);

    upper[i] = '\0';

    if (strcmp(upper, ".TEXT") == 0 || strcmp(upper, "TEXT") == 0)
        as->current_section = SEC_TEXT;
    else if (strcmp(upper, ".DATA") == 0 || strcmp(upper, "DATA") == 0)
        as->current_section = SEC_DATA;
    else if (strcmp(upper, ".BSS") == 0 || strcmp(upper, "BSS") == 0)
        as->current_section = SEC_BSS;
    else {
        fprintf(stderr, "[assembler] linea %d: seccion desconocida '%s'\n",
                node->line, name);
        as->warnings++;
    }
}

static void handle_data_directive(AssemblerState *as, const ASTNode *node) {
    int bytes_per_elem;

    switch (node->opcode) {
    case DIR_DB:
        bytes_per_elem = 1;
        break;
    case DIR_DW:
        bytes_per_elem = 2;
        break;
    case DIR_DD:
        bytes_per_elem = 4;
        break;
    default:
        return;
    }

    for (int i = 0; i < node->num_operands; i++) {
        const Operand *op = &node->operands[i];

        if (op->mode == ADDR_IMMEDIATE) {
            uint32_t val = (uint32_t)op->immediate;

            for (int b = 0; b < bytes_per_elem; b++)
                emit_byte(as, (uint8_t)((val >> (b * 8)) & 0xFF));
        } else if (op->mode == ADDR_LABEL && op->label) {
            int off = current_offset(as);
            emit_u32(as, 0x00000000);
            add_fixup(as, op->label, FIX_ABS32, off, off + 4);
        } else {
            fprintf(stderr,
                    "[assembler] linea %d: operando invalido en directiva de datos\n",
                    node->line);
            as->errors++;
        }
    }
}

static void handle_reserve_directive(AssemblerState *as, const ASTNode *node) {
    if (node->num_operands == 0)
        return;

    int bytes_per_elem;

    switch (node->opcode) {
    case DIR_RESB:
        bytes_per_elem = 1;
        break;
    case DIR_RESW:
        bytes_per_elem = 2;
        break;
    case DIR_RESD:
        bytes_per_elem = 4;
        break;
    default:
        return;
    }

    long count = node->operands[0].immediate;
    int total = (int)(count * bytes_per_elem);

    if (as->current_section == SEC_BSS) {
        as->sections[SEC_BSS].size += total;
    } else {
        for (int i = 0; i < total; i++)
            emit_byte(as, 0x00);
    }
}

static void handle_equ(AssemblerState *as, const ASTNode *node) {
    if (!node->label) {
        fprintf(stderr, "[assembler] linea %d: EQU sin etiqueta\n", node->line);
        as->errors++;
        return;
    }

    if (node->num_operands == 0)
        return;

    long value = node->operands[0].immediate;
    symbol_define(as, node->label, (uint32_t)value, as->current_section, SYM_EQU);
}

static void handle_org(AssemblerState *as, const ASTNode *node) {
    if (node->num_operands == 0)
        return;

    uint32_t addr = (uint32_t)node->operands[0].immediate;
    as->sections[as->current_section].base_addr = addr;
}

static void handle_instruction(AssemblerState *as, const ASTNode *node) {
    EncodedInstr enc = encode_instruction(node);

    if (enc.len == 0) {
        fprintf(stderr, "[assembler] linea %d: no se pudo codificar '%s'\n",
                node->line, opcode_name(node->opcode));
        as->errors++;
        return;
    }

    int instr_start = current_offset(as);
    emit_bytes(as, enc.bytes, enc.len);
    int instr_end = current_offset(as);

    if (enc.fixup_offset >= 0) {
        for (int i = 0; i < node->num_operands; i++) {
            if (node->operands[i].mode == ADDR_LABEL &&
                node->operands[i].label) {
                const char *sym = node->operands[i].label;
                Symbol *s = symbol_lookup(as, sym);
                FixupType ftype = enc.is_relative ? FIX_REL32 : FIX_ABS32;

                int needs_fixup = 0;
                if (ftype == FIX_ABS32) {
                    needs_fixup = 1;
                } else if (!s || !s->defined) {
                    needs_fixup = 1;
                } else if (s->type == SYM_EXTERN || s->type == SYM_GLOBAL) {
                    needs_fixup = 1;
                }

                if (needs_fixup) {
                    add_fixup(as, sym, ftype,
                              instr_start + enc.fixup_offset, instr_end);

                    if (!s) {
                        symbol_define(as, sym, 0,
                                      as->current_section, SYM_LOCAL);
                        Symbol *placeholder = symbol_lookup(as, sym);
                        if (placeholder) {
                            placeholder->defined = 0;
                        }
                    }
                } else {
                    uint32_t val = s->value - (uint32_t)instr_end;
                    patch_u32(as, as->current_section,
                              instr_start + enc.fixup_offset, val);
                }

                break;
            }
        }
    }
}

static void process_node(AssemblerState *as, const ASTNode *node,
                         int define_symbols) {
    if (node->label && define_symbols && node->opcode != DIR_EQU) {
        uint32_t addr = (uint32_t)(
            as->sections[as->current_section].base_addr +
            current_offset(as));

        symbol_define(as, node->label, addr, as->current_section, SYM_LOCAL);
    }

    switch (node->opcode) {
    case DIR_SECTION:
        handle_section(as, node);
        break;

    case DIR_GLOBAL:
        if (node->num_operands > 0 && node->operands[0].label) {
            Symbol *s = symbol_lookup(as, node->operands[0].label);

            if (s)
                s->type = SYM_GLOBAL;
            else
                symbol_define(as, node->operands[0].label,
                              0, as->current_section, SYM_GLOBAL);
        }
        break;

    case DIR_EXTERN:
        if (node->num_operands > 0 && node->operands[0].label)
            symbol_define(as, node->operands[0].label,
                          0, as->current_section, SYM_EXTERN);
        break;

    case DIR_DB:
    case DIR_DW:
    case DIR_DD:
        handle_data_directive(as, node);
        break;

    case DIR_RESB:
    case DIR_RESW:
    case DIR_RESD:
        handle_reserve_directive(as, node);
        break;

    case DIR_EQU:
        if (define_symbols) {
            handle_equ(as, node);
        }
        break;

    case DIR_ORG:
        handle_org(as, node);
        break;

    case OP_LABEL:
        break;

    default:
        handle_instruction(as, node);
        break;
    }
}

int assemble_one_pass(AssemblerState *as, const ParseResult *ast) {
    assembler_init(as);

    for (ASTNode *n = ast->head; n; n = n->next)
        process_node(as, n, 1);

    resolve_fixups(as);

    if (as->errors > 0) {
        fprintf(stderr, "[assembler] una pasada: %d error(es)\n", as->errors);
        return -1;
    }

    return 0;
}

static void first_pass(AssemblerState *as, const ParseResult *ast) {
    for (ASTNode *n = ast->head; n; n = n->next) {
        if (n->label && n->opcode != DIR_EQU) {
            uint32_t addr = (uint32_t)(
                as->sections[as->current_section].base_addr +
                current_offset(as));

            symbol_define(as, n->label, addr,
                          as->current_section, SYM_LOCAL);
        }

        if (n->opcode == DIR_SECTION) {
            handle_section(as, n);
            continue;
        }

        if (n->opcode == DIR_GLOBAL) {
            if (n->num_operands > 0 && n->operands[0].label)
                symbol_define(as, n->operands[0].label,
                              0, as->current_section, SYM_GLOBAL);
            continue;
        }

        if (n->opcode == DIR_EXTERN) {
            if (n->num_operands > 0 && n->operands[0].label)
                symbol_define(as, n->operands[0].label,
                              0, as->current_section, SYM_EXTERN);
            continue;
        }

        if (n->opcode == DIR_EQU) {
            handle_equ(as, n);
            continue;
        }

        if (n->opcode == DIR_ORG) {
            handle_org(as, n);
            continue;
        }

        if (n->opcode == DIR_DB)
            as->sections[as->current_section].size += n->num_operands * 1;
        else if (n->opcode == DIR_DW)
            as->sections[as->current_section].size += n->num_operands * 2;
        else if (n->opcode == DIR_DD)
            as->sections[as->current_section].size += n->num_operands * 4;
        else if (n->opcode == DIR_RESB || n->opcode == DIR_RESW ||
                 n->opcode == DIR_RESD) {
            int mul = (n->opcode == DIR_RESB) ? 1 :
                      (n->opcode == DIR_RESW) ? 2 : 4;

            if (n->num_operands > 0)
                as->sections[as->current_section].size +=
                    (int)(n->operands[0].immediate * mul);
        } else if (n->opcode != OP_LABEL && n->opcode != OP_UNKNOWN) {
            EncodedInstr enc = encode_instruction(n);
            as->sections[as->current_section].size += enc.len;
        }
    }
}

static void second_pass(AssemblerState *as, const ParseResult *ast) {
    for (int i = 0; i < MAX_SECTIONS; i++)
        as->sections[i].size = 0;

    as->current_section = SEC_TEXT;
    as->num_fixups = 0;

    for (ASTNode *n = ast->head; n; n = n->next)
        process_node(as, n, 0);

    resolve_fixups(as);
}

int assemble_two_pass(AssemblerState *as, const ParseResult *ast) {
    assembler_init(as);

    printf("[assembler] iniciando primera pasada...\n");
    first_pass(as, ast);

    if (as->errors > 0) {
        fprintf(stderr, "[assembler] primera pasada: %d error(es), abortando\n",
                as->errors);
        return -1;
    }

    printf("[assembler] iniciando segunda pasada...\n");
    second_pass(as, ast);

    if (as->errors > 0) {
        fprintf(stderr, "[assembler] segunda pasada: %d error(es)\n", as->errors);
        return -1;
    }

    return 0;
}

void assembler_free(AssemblerState *as) {
    memset(as, 0, sizeof(AssemblerState));
}

void print_symbol_table(const AssemblerState *as) {
    const char *type_names[] = {"LOCAL", "GLOBAL", "EXTERN", "EQU"};
    const char *sec_names[] = {".text", ".data", ".bss"};

    printf("\n=== TABLA DE SIMBOLOS (%d entradas) ===\n", as->num_symbols);
    printf("%-20s  %-8s  %-8s  %-7s  %s\n",
           "Nombre", "Valor", "Seccion", "Tipo", "Definido");
    printf("------------------------------------------------------\n");

    for (int i = 0; i < as->num_symbols; i++) {
        const Symbol *s = &as->symbols[i];

        printf("%-20s  %08X  %-8s  %-7s  %s\n",
               s->name,
               s->value,
               sec_names[s->section],
               type_names[s->type],
               s->defined ? "si" : "no");
    }

    printf("======================================\n\n");
}

void print_section_hex(const AssemblerState *as, SectionType sec) {
    const Section *s = &as->sections[sec];

    printf("\n=== SECCION %s (%d bytes) ===\n", s->name, s->size);

    for (int i = 0; i < s->size; i++) {
        if (i % 16 == 0)
            printf("  %04X: ", i);

        printf("%02X ", s->data[i]);

        if (i % 16 == 15)
            printf("\n");
    }

    if (s->size % 16 != 0)
        printf("\n");

    printf("================================\n\n");
}