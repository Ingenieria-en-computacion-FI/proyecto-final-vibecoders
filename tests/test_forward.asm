; Caso: Referencias adelantadas (forward references) + fixups
section .text
global _start
_start:
    jmp adelante       ; el destino aun no se conoce
    mov eax, 1
adelante:
    mov eax, 2
    call subrutina     ; llamada adelantada
    ret
subrutina:
    mov ebx, 7
    ret
