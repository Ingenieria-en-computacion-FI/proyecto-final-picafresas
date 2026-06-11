; test.asm — Caso de prueba básico
; cubre: MOV, ADD, etiquetas, saltos, DB, GLOBAL, EXTERN

SECTION text
GLOBAL  main
EXTERN  printf

main:
    MOV EAX, 10
    MOV EBX, 20
    ADD EAX, EBX
    CMP EAX, 0
    JE  fin
    MOV ECX, [EBP+4]
    MOV EDX, [EBX+ECX*4+8]
    CALL printf
    INC EAX
    DEC EBX
    NOP

fin:
    RET

SECTION data
mensaje DB 72, 111, 108, 97, 0
valor   DD 1234

SECTION bss
buffer  RESB 64
