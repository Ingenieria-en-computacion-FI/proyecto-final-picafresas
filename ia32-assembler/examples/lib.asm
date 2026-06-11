; lib.asm
SECTION text
GLOBAL calculo
GLOBAL print_msg

calculo:
    ADD EAX, EBX
    SUB EAX, 50
    IMUL EBX
    IDIV ECX
    RET

print_msg:
    MOV ECX, mensaje
    ; Simula llamada o salida
    NOP
    RET

SECTION data
mensaje DB 72, 101, 108, 108, 111, 0  ; "Hello"
