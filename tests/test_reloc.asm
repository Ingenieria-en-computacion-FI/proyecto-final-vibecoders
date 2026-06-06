; Caso: Relocaciones (referencias a datos y dd con simbolo)
section .data
dato:     dd 123
puntero:  dd dato        ; dd con simbolo -> ABS32 dentro de .data

section .text
global _start
_start:
    mov eax, [dato]      ; ABS32
    lea ebx, [dato]      ; ABS32
    mov ecx, [puntero]   ; ABS32
    ret
