; test_link_lib.asm
SECTION text
GLOBAL compute_sum

compute_sum:
    ADD EAX, EBX
    MOV EDX, global_var
    ADD EAX, EDX
    RET

SECTION data
GLOBAL global_var
global_var DD 100
