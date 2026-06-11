; test_addressing.asm
SECTION text
GLOBAL test_addr

test_addr:
    ; 1. Inmediato
    MOV EAX, 100

    ; 2. Registro a Registro
    MOV EAX, EBX

    ; 3. Memoria Directa
    MOV EAX, [1000]

    ; 4. Base + Desplazamiento
    MOV EAX, [EBP+4]
    MOV EBX, [ESP+16]

    ; 5. Base + Índice
    MOV EAX, [EBX+ECX]

    ; 6. Base + Índice Escalado (Escalas: 1, 2, 4, 8)
    MOV EAX, [EBX+ECX*1]
    MOV EAX, [EBX+ECX*2]
    MOV EAX, [EBX+ECX*4]
    MOV EAX, [EBX+ECX*8]

    ; 7. Base + Índice Escalado + Desplazamiento (SIB Completo)
    MOV EAX, [EBX+ECX*1+8]
    MOV EAX, [EBX+ECX*2+12]
    MOV EAX, [EBX+ECX*4+16]
    MOV EAX, [EBX+ECX*8+32]

    RET
