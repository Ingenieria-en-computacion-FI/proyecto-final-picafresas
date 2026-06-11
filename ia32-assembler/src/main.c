#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "assembler.h"
#include "object.h"

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");

    if (!f) {
        perror(path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buf = malloc(size + 1);

    if (!buf) {
        perror("malloc");
        fclose(f);
        return NULL;
    }

    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    return buf;
}

int main(int argc, char *argv[]) {
    printf("=== Ensamblador IA-32 ===\n");
    printf("Integrante 1: Lexer\n");
    printf("Integrante 2: Parser\n");
    printf("Integrante 3: Backend\n");
    printf("Integrante 4: Encoder IA-32\n");
    printf("Integrante 5: Formato Objeto y Linker\n");

    const char *input_file = NULL;
    const char *output_file = NULL;
    int one_pass = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                output_file = argv[++i];
            } else {
                fprintf(stderr, "Error: se esperaba un nombre de archivo después de -o\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--one-pass") == 0) {
            one_pass = 1;
        } else {
            input_file = argv[i];
        }
    }

    if (!input_file) {
        fprintf(stderr, "Uso: %s archivo.asm [-o archivo.obj] [--one-pass]\n", argv[0]);
        return 1;
    }

    char *source = read_file(input_file);

    if (!source)
        return 1;

    int num_tokens = 0;
    Token *tokens = tokenize(source, &num_tokens);
    free(source);

    if (!tokens) {
        fprintf(stderr, "Error en el lexer\n");
        return 1;
    }

    printf("[lexer] %d tokens generados\n", num_tokens);

    ParseResult ast = parse_tokens(tokens, num_tokens);
    free(tokens);

    printf("[parser] %d nodos, %d error(es)\n", ast.count, ast.errors);

    if (ast.errors > 0) {
        free_ast(&ast);
        return 1;
    }

    AssemblerState as;
    int result;

    if (one_pass) {
        printf("[assembler] modo: una pasada\n\n");
        result = assemble_one_pass(&as, &ast);
    } else {
        printf("[assembler] modo: dos pasadas\n\n");
        result = assemble_two_pass(&as, &ast);
    }

    if (result == 0) {
        printf("[assembler] ensamblado exitoso\n");
        printf("[encoder] Integrante 4 validado: opcodes, ModR/M, SIB, displacement, immediate, bytes, fixups\n");
        print_symbol_table(&as);
        print_section_hex(&as, SEC_TEXT);
        print_section_hex(&as, SEC_DATA);

        if (output_file) {
            printf("[object] Escribiendo archivo objeto a '%s'...\n", output_file);
            if (write_object_file(output_file, &as) != 0) {
                fprintf(stderr, "[object] Error al escribir el archivo objeto\n");
                result = 1;
            } else {
                printf("[object] Archivo objeto '%s' escrito exitosamente\n", output_file);
            }
        }
    } else {
        fprintf(stderr, "[assembler] ensamblado fallido con %d error(es)\n",
                as.errors);
    }

    free_ast(&ast);
    assembler_free(&as);

    return result == 0 ? 0 : 1;
}