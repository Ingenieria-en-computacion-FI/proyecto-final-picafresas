; test_error_semantic.asm
SECTION text

start:
    ; 1. Referencia a etiqueta local no definida
    JMP non_existent_label

    ; 2. Redefinición de etiqueta
dup_label:
    NOP
dup_label:
    RET

    ; 3. EQU sin etiqueta
    EQU 42
