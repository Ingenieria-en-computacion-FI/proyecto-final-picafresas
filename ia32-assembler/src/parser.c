#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"

typedef struct {
    Token *tokens;
    int num_tokens;
    int pos;
    int errors;
} Parser;

static Token *current(Parser *p) {
    if (p->pos < p->num_tokens)
        return &p->tokens[p->pos];

    return &p->tokens[p->num_tokens - 1];
}

static Token *peek(Parser *p, int offset) {
    int idx = p->pos + offset;

    if (idx < p->num_tokens)
        return &p->tokens[idx];

    return &p->tokens[p->num_tokens - 1];
}

static Token *advance(Parser *p) {
    Token *t = current(p);

    if (p->pos < p->num_tokens - 1)
        p->pos++;

    return t;
}

static int check(Parser *p, TokenType type) {
    return current(p)->type == type;
}

static int match(Parser *p, TokenType type) {
    if (check(p, type)) {
        advance(p);
        return 1;
    }

    return 0;
}

static void skip_newlines(Parser *p) {
    while (check(p, TOK_NEWLINE))
        advance(p);
}

static void error_at(Parser *p, Token *t, const char *msg) {
    fprintf(stderr, "[parser] linea %d col %d: %s (token '%s')\n",
            t->line, t->col, msg, (t->value[0] != '\0' ? t->value : ""));
    p->errors++;
}

typedef struct {
    const char *name;
    Opcode op;
} MnemonicEntry;

static const MnemonicEntry MNEMONIC_TABLE[] = {
    {"MOV", OP_MOV}, {"PUSH", OP_PUSH}, {"POP", OP_POP}, {"LEA", OP_LEA},
    {"ADD", OP_ADD}, {"SUB", OP_SUB}, {"INC", OP_INC}, {"DEC", OP_DEC},
    {"CMP", OP_CMP}, {"NEG", OP_NEG}, {"MUL", OP_MUL}, {"DIV", OP_DIV}, {"IMUL", OP_IMUL}, {"IDIV", OP_IDIV},
    {"AND", OP_AND}, {"OR", OP_OR}, {"XOR", OP_XOR}, {"NOT", OP_NOT},
    {"JMP", OP_JMP}, {"JE", OP_JE}, {"JNE", OP_JNE}, {"JG", OP_JG},
    {"JL", OP_JL}, {"JGE", OP_JGE}, {"JLE", OP_JLE},
    {"CALL", OP_CALL}, {"RET", OP_RET}, {"NOP", OP_NOP}, {"INT", OP_INT},
    {"SECTION", DIR_SECTION}, {"GLOBAL", DIR_GLOBAL}, {"EXTERN", DIR_EXTERN},
    {"DB", DIR_DB}, {"DW", DIR_DW}, {"DD", DIR_DD},
    {"ORG", DIR_ORG}, {"EQU", DIR_EQU},
    {"RESB", DIR_RESB}, {"RESW", DIR_RESW}, {"RESD", DIR_RESD},
    {NULL, OP_UNKNOWN}
};

static Opcode lookup_mnemonic(const char *name) {
    char upper[32];
    int i;

    for (i = 0; name[i] && i < 31; i++)
        upper[i] = (char)toupper((unsigned char)name[i]);

    upper[i] = '\0';

    for (const MnemonicEntry *e = MNEMONIC_TABLE; e->name; e++) {
        if (strcmp(e->name, upper) == 0)
            return e->op;
    }

    return OP_UNKNOWN;
}

typedef struct {
    const char *name;
    Register reg;
} RegEntry;

static const RegEntry REG_TABLE[] = {
    {"EAX", REG_EAX}, {"ECX", REG_ECX}, {"EDX", REG_EDX}, {"EBX", REG_EBX},
    {"ESP", REG_ESP}, {"EBP", REG_EBP}, {"ESI", REG_ESI}, {"EDI", REG_EDI},
    {NULL, REG_NONE}
};

static Register lookup_register(const char *name) {
    char upper[8];
    int i;

    for (i = 0; name[i] && i < 7; i++)
        upper[i] = (char)toupper((unsigned char)name[i]);

    upper[i] = '\0';

    for (const RegEntry *e = REG_TABLE; e->name; e++) {
        if (strcmp(e->name, upper) == 0)
            return e->reg;
    }

    return REG_NONE;
}

static int is_register(const char *name) {
    return lookup_register(name) != REG_NONE;
}

static ASTNode *new_node(Opcode op, int line) {
    ASTNode *n = calloc(1, sizeof(ASTNode));

    if (!n) {
        perror("calloc");
        exit(EXIT_FAILURE);
    }

    n->opcode = op;
    n->line = line;

    return n;
}

static void append_node(ParseResult *r, ASTNode *n) {
    if (!r->head) {
        r->head = r->tail = n;
    } else {
        r->tail->next = n;
        r->tail = n;
    }

    r->count++;
}

static char *strdup_safe(const char *s) {
    if (!s)
        return NULL;

    char *d = malloc(strlen(s) + 1);

    if (!d) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    return strcpy(d, s);
}

static int parse_memory_operand(Parser *p, Operand *op) {
    if (!check(p, TOK_LBRACKET))
        return 0;

    advance(p);

    op->reg = REG_NONE;
    op->index = REG_NONE;
    op->scale = 1;
    op->displacement = 0;

    Token *t = current(p);

    if (t->type == TOK_NUMBER) {
        op->mode = ADDR_DIRECT;
        op->displacement = t->ival;
        advance(p);
    } else if (t->type == TOK_IDENT && is_register(t->value)) {
        op->reg = lookup_register(t->value);
        advance(p);

        if (check(p, TOK_RBRACKET)) {
            op->mode = ADDR_BASE_DISP;
            op->displacement = 0;
        } else if (check(p, TOK_PLUS) || check(p, TOK_MINUS)) {
            int sign = check(p, TOK_MINUS) ? -1 : 1;
            advance(p);

            Token *t2 = current(p);

            if (t2->type == TOK_IDENT && is_register(t2->value)) {
                op->index = lookup_register(t2->value);
                advance(p);

                if (check(p, TOK_STAR)) {
                    advance(p);

                    if (!check(p, TOK_NUMBER)) {
                        error_at(p, current(p), "se esperaba escala (1,2,4,8)");
                        return 0;
                    }

                    op->scale = (int)current(p)->ival;

                    if (op->scale != 1 && op->scale != 2 &&
                        op->scale != 4 && op->scale != 8) {
                        error_at(p, current(p), "escala invalida, debe ser 1,2,4 u 8");
                        op->scale = 1;
                    }

                    advance(p);

                    if (check(p, TOK_PLUS) || check(p, TOK_MINUS)) {
                        int sign2 = check(p, TOK_MINUS) ? -1 : 1;
                        advance(p);

                        if (!check(p, TOK_NUMBER)) {
                            error_at(p, current(p), "se esperaba desplazamiento");
                            return 0;
                        }

                        op->displacement = sign2 * current(p)->ival;
                        advance(p);
                        op->mode = ADDR_SIB_DISP;
                    } else {
                        op->mode = ADDR_BASE_INDEX_SCALED;
                    }
                } else if (check(p, TOK_PLUS) || check(p, TOK_MINUS)) {
                    int sign2 = check(p, TOK_MINUS) ? -1 : 1;
                    advance(p);

                    if (!check(p, TOK_NUMBER)) {
                        error_at(p, current(p), "se esperaba desplazamiento");
                        return 0;
                    }

                    op->displacement = sign2 * current(p)->ival;
                    advance(p);
                    op->mode = ADDR_SIB_DISP;
                    op->scale = 1;
                } else {
                    op->mode = ADDR_BASE_INDEX;
                }
            } else if (t2->type == TOK_NUMBER) {
                op->displacement = sign * t2->ival;
                advance(p);
                op->mode = ADDR_BASE_DISP;
            } else {
                error_at(p, t2, "se esperaba registro o numero tras '+'/'-'");
                return 0;
            }
        } else {
            error_at(p, current(p), "token inesperado dentro de '[]'");
            return 0;
        }
    } else {
        error_at(p, t, "expresion de memoria invalida");
        return 0;
    }

    if (!check(p, TOK_RBRACKET)) {
        error_at(p, current(p), "se esperaba ']'");
        return 0;
    }

    advance(p);
    return 1;
}

static int parse_operand(Parser *p, Operand *op) {
    Token *t = current(p);

    memset(op, 0, sizeof(Operand));
    op->reg = REG_NONE;
    op->index = REG_NONE;
    op->scale = 1;

    switch (t->type) {
    case TOK_NUMBER:
        op->mode = ADDR_IMMEDIATE;
        op->immediate = t->ival;
        advance(p);
        return 1;

    case TOK_DOT: {
        advance(p);

        if (!check(p, TOK_IDENT)) {
            error_at(p, current(p), "se esperaba identificador tras '.'");
            return 0;
        }

        op->mode = ADDR_LABEL;

        size_t len = strlen(current(p)->value) + 2;
        char *nombre = malloc(len);

        if (!nombre) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        snprintf(nombre, len, ".%s", current(p)->value);
        op->label = nombre;
        advance(p);
        return 1;
    }

    case TOK_MINUS:
        advance(p);

        if (!check(p, TOK_NUMBER)) {
            error_at(p, current(p), "se esperaba numero tras '-'");
            return 0;
        }

        op->mode = ADDR_IMMEDIATE;
        op->immediate = -(current(p)->ival);
        advance(p);
        return 1;

    case TOK_LBRACKET:
        return parse_memory_operand(p, op);

    case TOK_IDENT: {
        Register r = lookup_register(t->value);

        if (r != REG_NONE) {
            op->mode = ADDR_REGISTER;
            op->reg = r;
            advance(p);
            return 1;
        }

        op->mode = ADDR_LABEL;
        op->label = strdup_safe(t->value);
        advance(p);
        return 1;
    }

    default:
        error_at(p, t, "operando invalido");
        return 0;
    }
}

static int expected_operands(Opcode op) {
    switch (op) {
    case OP_RET:
    case OP_NOP:
        return 0;

    case OP_PUSH:
    case OP_POP:
    case OP_INC:
    case OP_DEC:
    case OP_NEG:
    case OP_NOT:
    case OP_MUL:
    case OP_DIV:
    case OP_IMUL:
    case OP_IDIV:
    case OP_JMP:
    case OP_JE:
    case OP_JNE:
    case OP_JG:
    case OP_JL:
    case OP_JGE:
    case OP_JLE:
    case OP_CALL:
    case OP_INT:
        return 1;

    case OP_MOV:
    case OP_LEA:
    case OP_ADD:
    case OP_SUB:
    case OP_CMP:
    case OP_AND:
    case OP_OR:
    case OP_XOR:
        return 2;

    default:
        return -1;
    }
}

static void parse_data_list(Parser *p, ASTNode *node) {
    while (!check(p, TOK_NEWLINE) && !check(p, TOK_EOF) &&
           node->num_operands < MAX_OPERANDS) {
        if (node->num_operands > 0) {
            if (!match(p, TOK_COMMA))
                break;
        }

        Operand op;

        if (!parse_operand(p, &op))
            break;

        node->operands[node->num_operands++] = op;
    }
}

static ASTNode *parse_instruction(Parser *p, char *label_before) {
    Token *t = current(p);
    int line = t->line;

    Opcode op = lookup_mnemonic(t->value);

    if (op == OP_UNKNOWN) {
        error_at(p, t, "mnemonico desconocido");
        advance(p);
        return NULL;
    }

    advance(p);

    ASTNode *node = new_node(op, line);
    node->label = label_before;

    if (op == OP_RET || op == OP_NOP)
        return node;

    if (op == DIR_DB || op == DIR_DW || op == DIR_DD) {
        parse_data_list(p, node);
        return node;
    }

    if (op == DIR_RESB || op == DIR_RESW || op == DIR_RESD ||
        op == DIR_EQU || op == DIR_GLOBAL || op == DIR_EXTERN ||
        op == DIR_SECTION) {
        Operand op0;

        if (parse_operand(p, &op0)) {
            node->operands[0] = op0;
            node->num_operands = 1;
        }

        return node;
    }

    if (op == DIR_ORG) {
        if (!check(p, TOK_NUMBER)) {
            error_at(p, current(p), "ORG requiere una direccion numerica");
        } else {
            Operand op0;

            memset(&op0, 0, sizeof(Operand));
            op0.reg = REG_NONE;
            op0.index = REG_NONE;
            op0.scale = 1;
            op0.mode = ADDR_IMMEDIATE;
            op0.immediate = current(p)->ival;

            advance(p);

            node->operands[0] = op0;
            node->num_operands = 1;
        }

        return node;
    }

    int max_ops = expected_operands(op);

    while (!check(p, TOK_NEWLINE) && !check(p, TOK_EOF) &&
           node->num_operands < MAX_OPERANDS) {
        if (node->num_operands > 0) {
            if (!match(p, TOK_COMMA)) {
                error_at(p, current(p), "se esperaba ','");
                break;
            }
        }

        Operand oper;

        if (!parse_operand(p, &oper))
            break;

        node->operands[node->num_operands++] = oper;
    }

    if (max_ops >= 0 && node->num_operands != max_ops) {
        fprintf(stderr,
                "[parser] linea %d: %s espera %d operandos, se encontraron %d\n",
                line, opcode_name(op), max_ops, node->num_operands);
        p->errors++;
    }

    return node;
}

ParseResult parse_tokens(Token *tokens, int num_tokens) {
    ParseResult result = {NULL, NULL, 0, 0};

    Parser p;
    p.tokens = tokens;
    p.num_tokens = num_tokens;
    p.pos = 0;
    p.errors = 0;

    while (!check(&p, TOK_EOF)) {
        skip_newlines(&p);

        if (check(&p, TOK_EOF))
            break;

        Token *t = current(&p);
        char *pending_label = NULL;

        if (t->type == TOK_IDENT && peek(&p, 1)->type == TOK_COLON) {
            pending_label = strdup_safe(t->value);

            advance(&p);
            advance(&p);

            if (check(&p, TOK_NEWLINE) || check(&p, TOK_EOF)) {
                ASTNode *ln = new_node(OP_LABEL, t->line);
                ln->label = pending_label;
                append_node(&result, ln);
                continue;
            }
        } else if (t->type == TOK_IDENT &&
                   (lookup_mnemonic(peek(&p, 1)->value) == DIR_DB ||
                    lookup_mnemonic(peek(&p, 1)->value) == DIR_DW ||
                    lookup_mnemonic(peek(&p, 1)->value) == DIR_DD ||
                    lookup_mnemonic(peek(&p, 1)->value) == DIR_RESB ||
                    lookup_mnemonic(peek(&p, 1)->value) == DIR_RESW ||
                    lookup_mnemonic(peek(&p, 1)->value) == DIR_RESD ||
                    lookup_mnemonic(peek(&p, 1)->value) == DIR_EQU)) {
            pending_label = strdup_safe(t->value);
            advance(&p);
            t = current(&p);
        }

        if (check(&p, TOK_DOT)) {
            advance(&p);

            if (!check(&p, TOK_IDENT)) {
                error_at(&p, current(&p), "se esperaba nombre de directiva tras '.'");
                free(pending_label);
                skip_newlines(&p);
                continue;
            }
        }

        if (check(&p, TOK_IDENT)) {
            ASTNode *node = parse_instruction(&p, pending_label);

            if (node)
                append_node(&result, node);
            else
                free(pending_label);
        } else {
            error_at(&p, current(&p), "token inesperado al inicio de linea");
            advance(&p);
            free(pending_label);
        }

        if (!match(&p, TOK_NEWLINE) && !check(&p, TOK_EOF)) {
            while (!check(&p, TOK_NEWLINE) && !check(&p, TOK_EOF))
                advance(&p);
        }
    }

    result.errors = p.errors;
    return result;
}

void free_ast(ParseResult *result) {
    ASTNode *n = result->head;

    while (n) {
        ASTNode *next = n->next;

        free(n->label);

        for (int i = 0; i < n->num_operands; i++)
            free(n->operands[i].label);

        free(n);
        n = next;
    }

    result->head = NULL;
    result->tail = NULL;
    result->count = 0;
    result->errors = 0;
}

const char *opcode_name(Opcode op) {
    switch (op) {
    case OP_MOV:
        return "MOV";
    case OP_PUSH:
        return "PUSH";
    case OP_POP:
        return "POP";
    case OP_LEA:
        return "LEA";
    case OP_ADD:
        return "ADD";
    case OP_SUB:
        return "SUB";
    case OP_INC:
        return "INC";
    case OP_DEC:
        return "DEC";
    case OP_CMP:
        return "CMP";
    case OP_NEG:
        return "NEG";
    case OP_MUL:
        return "MUL";
    case OP_DIV:
        return "DIV";
    case OP_IMUL:
        return "IMUL";
    case OP_IDIV:
        return "IDIV";
    case OP_AND:
        return "AND";
    case OP_OR:
        return "OR";
    case OP_XOR:
        return "XOR";
    case OP_NOT:
        return "NOT";
    case OP_JMP:
        return "JMP";
    case OP_JE:
        return "JE";
    case OP_JNE:
        return "JNE";
    case OP_JG:
        return "JG";
    case OP_JL:
        return "JL";
    case OP_JGE:
        return "JGE";
    case OP_JLE:
        return "JLE";
    case OP_CALL:
        return "CALL";
    case OP_RET:
        return "RET";
    case OP_NOP:
        return "NOP";
    case OP_INT:
        return "INT";
    case OP_LABEL:
        return "LABEL";
    case DIR_SECTION:
        return "SECTION";
    case DIR_GLOBAL:
        return "GLOBAL";
    case DIR_EXTERN:
        return "EXTERN";
    case DIR_DB:
        return "DB";
    case DIR_DW:
        return "DW";
    case DIR_DD:
        return "DD";
    case DIR_ORG:
        return "ORG";
    case DIR_EQU:
        return "EQU";
    case DIR_RESB:
        return "RESB";
    case DIR_RESW:
        return "RESW";
    case DIR_RESD:
        return "RESD";
    default:
        return "UNKNOWN";
    }
}

const char *addr_mode_name(AddressingMode mode) {
    switch (mode) {
    case ADDR_IMMEDIATE:
        return "inmediato";
    case ADDR_REGISTER:
        return "registro";
    case ADDR_DIRECT:
        return "memoria directa";
    case ADDR_BASE_DISP:
        return "base+disp";
    case ADDR_BASE_INDEX:
        return "base+index";
    case ADDR_BASE_INDEX_SCALED:
        return "base+index*scale";
    case ADDR_SIB_DISP:
        return "SIB+disp";
    case ADDR_LABEL:
        return "etiqueta";
    default:
        return "desconocido";
    }
}

const char *register_name(Register reg) {
    switch (reg) {
    case REG_EAX:
        return "EAX";
    case REG_ECX:
        return "ECX";
    case REG_EDX:
        return "EDX";
    case REG_EBX:
        return "EBX";
    case REG_ESP:
        return "ESP";
    case REG_EBP:
        return "EBP";
    case REG_ESI:
        return "ESI";
    case REG_EDI:
        return "EDI";
    default:
        return "NONE";
    }
}

static void print_operand(const Operand *op) {
    printf("    modo: %s", addr_mode_name(op->mode));

    switch (op->mode) {
    case ADDR_IMMEDIATE:
        printf(" | valor=%ld", op->immediate);
        break;
    case ADDR_REGISTER:
        printf(" | reg=%s", register_name(op->reg));
        break;
    case ADDR_DIRECT:
        printf(" | addr=0x%lx", op->displacement);
        break;
    case ADDR_BASE_DISP:
        printf(" | base=%s disp=%ld",
               register_name(op->reg), op->displacement);
        break;
    case ADDR_BASE_INDEX:
        printf(" | base=%s idx=%s",
               register_name(op->reg), register_name(op->index));
        break;
    case ADDR_BASE_INDEX_SCALED:
        printf(" | base=%s idx=%s scale=%d",
               register_name(op->reg), register_name(op->index), op->scale);
        break;
    case ADDR_SIB_DISP:
        printf(" | base=%s idx=%s scale=%d disp=%ld",
               register_name(op->reg), register_name(op->index),
               op->scale, op->displacement);
        break;
    case ADDR_LABEL:
        printf(" | sym=%s", op->label ? op->label : "(null)");
        break;
    }

    printf("\n");
}

void print_ast(const ParseResult *result) {
    printf("=== AST (%d nodos, %d errores) ===\n",
           result->count, result->errors);

    for (ASTNode *n = result->head; n; n = n->next) {
        printf("[linea %3d] %-8s", n->line, opcode_name(n->opcode));

        if (n->label)
            printf(" label='%s'", n->label);

        printf(" (%d operandos)\n", n->num_operands);

        for (int i = 0; i < n->num_operands; i++)
            print_operand(&n->operands[i]);
    }

    printf("===================================\n");
}