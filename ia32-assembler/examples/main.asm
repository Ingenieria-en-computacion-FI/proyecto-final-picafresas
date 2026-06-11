; main.asm
SECTION text
GLOBAL _start
EXTERN print_msg
EXTERN calculo

_start:
    MOV EAX, 100
    MOV EBX, 200
    CALL calculo
    MOV ECX, EAX
    CALL print_msg
    RET

SECTION data
resultado DD 0
