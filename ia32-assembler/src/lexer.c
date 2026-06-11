#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"

static Token tokens_buf[MAX_TOKENS];
static int token_count = 0;

const char *tokenTypeToString(TokenType type) {
    switch (type) {
    case TOK_NUMBER:
        return "NUMBER";
    case TOK_IDENT:
        return "IDENT";
    case TOK_COMMA:
        return "COMMA";
    case TOK_COLON:
        return "COLON";
    case TOK_LBRACKET:
        return "LBRACKET";
    case TOK_RBRACKET:
        return "RBRACKET";
    case TOK_PLUS:
        return "PLUS";
    case TOK_MINUS:
        return "MINUS";
    case TOK_STAR:
        return "STAR";
    case TOK_DOT:
        return "DOT";
    case TOK_NEWLINE:
        return "NEWLINE";
    case TOK_EOF:
        return "EOF";
    default:
        return "UNKNOWN";
    }
}

static void add_token(TokenType type, const char *value, long ival,
                      int line, int col) {
    if (token_count >= MAX_TOKENS) {
        fprintf(stderr, "[lexer] demasiados tokens (max %d)\n", MAX_TOKENS);
        return;
    }

    Token *t = &tokens_buf[token_count++];

    t->type = type;
    strncpy(t->value, value, MAX_VALUE - 1);
    t->value[MAX_VALUE - 1] = '\0';
    t->ival = ival;
    t->line = line;
    t->col = col;
}

char *readFile(const char *fileName) {
    FILE *file = fopen(fileName, "rb");

    if (!file) {
        fprintf(stderr, "[lexer] no se pudo abrir '%s'\n", fileName);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);

    char *content = malloc(size + 1);

    if (!content) {
        fprintf(stderr, "[lexer] sin memoria\n");
        fclose(file);
        return NULL;
    }

    fread(content, 1, size, file);
    content[size] = '\0';
    fclose(file);

    return content;
}

static void run_lexer(const char *source) {
    int i = 0;
    int line = 1;
    int col = 1;

    while (source[i] != '\0') {
        char c = source[i];

        if (c == ' ' || c == '\t' || c == '\r') {
            i++;
            col++;
            continue;
        }

        if (c == '\n') {
            add_token(TOK_NEWLINE, "\\n", 0, line, col);
            i++;
            line++;
            col = 1;
            continue;
        }

        if (c == ';') {
            while (source[i] != '\0' && source[i] != '\n') {
                i++;
                col++;
            }
            continue;
        }

        if (c == ',') {
            add_token(TOK_COMMA, ",", 0, line, col);
            i++;
            col++;
            continue;
        }

        if (c == ':') {
            add_token(TOK_COLON, ":", 0, line, col);
            i++;
            col++;
            continue;
        }

        if (c == '[') {
            add_token(TOK_LBRACKET, "[", 0, line, col);
            i++;
            col++;
            continue;
        }

        if (c == ']') {
            add_token(TOK_RBRACKET, "]", 0, line, col);
            i++;
            col++;
            continue;
        }

        if (c == '+') {
            add_token(TOK_PLUS, "+", 0, line, col);
            i++;
            col++;
            continue;
        }

        if (c == '-') {
            add_token(TOK_MINUS, "-", 0, line, col);
            i++;
            col++;
            continue;
        }

        if (c == '*') {
            add_token(TOK_STAR, "*", 0, line, col);
            i++;
            col++;
            continue;
        }

        if (c == '.') {
            add_token(TOK_DOT, ".", 0, line, col);
            i++;
            col++;
            continue;
        }

        if (isdigit((unsigned char)c)) {
            char number[MAX_VALUE];
            int start_col = col;
            int j = 0;

            if (c == '0' && (source[i + 1] == 'x' || source[i + 1] == 'X')) {
                number[j++] = source[i++];
                col++;
                number[j++] = source[i++];
                col++;

                while (isxdigit((unsigned char)source[i])) {
                    number[j++] = source[i++];
                    col++;
                }
            } else {
                while (isdigit((unsigned char)source[i])) {
                    number[j++] = source[i++];
                    col++;
                }
            }

            number[j] = '\0';

            long ival = strtol(number, NULL, 0);
            add_token(TOK_NUMBER, number, ival, line, start_col);
            continue;
        }

        if (isalpha((unsigned char)c) || c == '_') {
            char word[MAX_VALUE];
            int start_col = col;
            int j = 0;

            while (isalnum((unsigned char)source[i]) || source[i] == '_') {
                word[j++] = source[i++];
                col++;
            }

            word[j] = '\0';

            add_token(TOK_IDENT, word, 0, line, start_col);
            continue;
        }

        fprintf(stderr, "[lexer] linea %d col %d: caracter no valido '%c'\n",
                line, col, c);

        i++;
        col++;
    }

    add_token(TOK_EOF, "EOF", 0, line, col);
}

Token *tokenize(const char *source, int *out_count) {
    token_count = 0;
    run_lexer(source);

    Token *result = malloc(sizeof(Token) * token_count);

    if (!result) {
        fprintf(stderr, "[lexer] sin memoria para tokens\n");
        *out_count = 0;
        return NULL;
    }

    memcpy(result, tokens_buf, sizeof(Token) * token_count);
    *out_count = token_count;

    return result;
}