#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

typedef enum {
    OP_MOV, OP_PUSH, OP_POP, OP_LEA,
    OP_ADD, OP_SUB, OP_INC, OP_DEC,
    OP_CMP, OP_NEG, OP_MUL, OP_DIV, OP_IMUL, OP_IDIV,
    OP_AND, OP_OR, OP_XOR, OP_NOT,
    OP_JMP, OP_JE, OP_JNE, OP_JG,
    OP_JL, OP_JGE, OP_JLE,
    OP_CALL, OP_RET, OP_NOP, OP_INT,
    OP_LABEL,
    DIR_SECTION, DIR_GLOBAL, DIR_EXTERN,
    DIR_DB, DIR_DW, DIR_DD,
    DIR_ORG, DIR_EQU,
    DIR_RESB, DIR_RESW, DIR_RESD,
    OP_UNKNOWN
} Opcode;

typedef enum {
    REG_EAX, REG_ECX, REG_EDX, REG_EBX,
    REG_ESP, REG_EBP, REG_ESI, REG_EDI,
    REG_NONE
} Register;

typedef enum {
    ADDR_IMMEDIATE,
    ADDR_REGISTER,
    ADDR_DIRECT,
    ADDR_BASE_DISP,
    ADDR_BASE_INDEX,
    ADDR_BASE_INDEX_SCALED,
    ADDR_SIB_DISP,
    ADDR_LABEL
} AddressingMode;

typedef struct {
    AddressingMode mode;
    Register reg;
    Register index;
    int scale;
    long displacement;
    long immediate;
    char *label;
} Operand;

#define MAX_OPERANDS 8

typedef struct ASTNode {
    Opcode opcode;
    char *label;
    int num_operands;
    Operand operands[MAX_OPERANDS];
    int line;
    struct ASTNode *next;
} ASTNode;

typedef struct {
    ASTNode *head;
    ASTNode *tail;
    int count;
    int errors;
} ParseResult;

ParseResult parse_tokens(Token *tokens, int num_tokens);

void free_ast(ParseResult *result);

void print_ast(const ParseResult *result);

const char *opcode_name(Opcode op);

const char *addr_mode_name(AddressingMode mode);

const char *register_name(Register reg);

#endif