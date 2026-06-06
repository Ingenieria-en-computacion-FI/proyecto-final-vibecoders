; Caso: EXTERN (simbolos externos -> relocaciones)
section .text
global _start
extern externo
_start:
    call externo       ; REL32 hacia simbolo externo
    mov  eax, externo  ; ABS32 (direccion del externo)
    ret
