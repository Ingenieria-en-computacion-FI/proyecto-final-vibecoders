; Caso: CALL a una etiqueta local
section .text
global _start
_start:
    call funcion
    add  eax, 1
    ret
funcion:
    mov  eax, 42
    ret
