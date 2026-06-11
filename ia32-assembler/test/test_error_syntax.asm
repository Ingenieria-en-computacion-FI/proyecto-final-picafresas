SECTION text

main:
    ; caracter inválido en lexer
    MOV EAX, @123

    ; escala invalida en direccionamiento 
    MOV EAX, [EBX+ECX*3]

    ; falta de coma entre operandos
    MOV EAX EBX

    ; numero incorrecto de operandos
    MOV EAX
    RET EAX, EBX

    ; mnemonico desconocido
    BADINSTR EBX, 2
