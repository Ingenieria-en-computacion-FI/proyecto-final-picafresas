; test_instructions.asm
SECTION text
GLOBAL test_inst

test_inst:
    ; 1. Transferencia
    MOV EAX, 42
    MOV EBX, EAX
    PUSH ECX
    POP EDX
    LEA ESI, [EBP+4]

    ; 2. Aritméticas
    ADD EAX, EBX
    ADD EAX, 10
    SUB ECX, EDX
    SUB ECX, 5
    INC EAX
    DEC EBX
    CMP EAX, EBX
    NEG EAX
    MUL EBX
    DIV ECX
    IMUL EDX
    IDIV ESI

    ; 3. Lógicas
    AND EAX, EBX
    AND EAX, 0xF
    OR ECX, EDX
    OR ECX, 0x10
    XOR ESI, EDI
    XOR ESI, 0x0
    NOT EAX

    ; 4. Saltos y control
    JMP loop_start

loop_start:
    JE end_label
    JNE loop_start
    JG end_label
    JL loop_start
    JGE end_label
    JLE loop_start
    CALL test_inst
    INT 0x80
    NOP
    RET

end_label:
    RET
