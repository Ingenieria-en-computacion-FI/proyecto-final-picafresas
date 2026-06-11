#ifndef LEXER_H
#define LEXER_H

#include <stdlib.h>

#define MAX_VALUE 100
#define MAX_TOKENS 4096

typedef enum {
    TOK_NUMBER,
    TOK_IDENT,
    TOK_COMMA,
    TOK_COLON,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_DOT,
    TOK_NEWLINE,
    TOK_EOF
} TokenType;

typedef struct {
    TokenType type;
    char value[MAX_VALUE];
    long ival;
    int line;
    int col;
} Token;

Token *tokenize(const char *source, int *out_count);

#endif