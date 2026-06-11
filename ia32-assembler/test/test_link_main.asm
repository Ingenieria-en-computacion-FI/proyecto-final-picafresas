; test_link_main.asm
SECTION text
GLOBAL _start
EXTERN compute_sum
EXTERN global_var

_start:
    MOV EAX, 10
    MOV EBX, 20
    CALL compute_sum
    MOV ECX, global_var
    RET

SECTION data
local_val DD 1234
