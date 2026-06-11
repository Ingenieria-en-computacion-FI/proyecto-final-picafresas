; test_one_pass.asm
SECTION text
GLOBAL main_entry

main_entry:
    ; referencias adelantadas 
    JMP forward_target
    CALL print_fn
    MOV EAX, forward_target
    RET

forward_target:
    ADD EAX, EBX
    RET

print_fn:
    NOP
    RET
