; test_directives.asm
ORG 0x1000

SECTION text
GLOBAL entry_point
EXTERN external_func

MY_CONST EQU 0x55

entry_point:
    MOV EAX, MY_CONST
    CALL external_func
    RET

SECTION data
    ; Directivas de definición de datos
    val_db DB 1, 2, 3, 4
    val_dw DW 0x1234, 0x5678
    val_dd DD 0x11223344, 0x55667788
    msg    DB 72, 101, 108, 108, 111, 0  ; "Hello"
    addr_ref DD entry_point               ; Referencia absoluta de dirección

SECTION bss
    ; Directivas de reserva de espacio
    res_b  RESB 10
    res_w  RESW 5
    res_d  RESD 2
