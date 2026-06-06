; Caso: MOV inmediato
section .text
global _start
_start:
    mov eax, 10        ; decimal
    mov ebx, 0x1F      ; hexadecimal
    mov ecx, -5        ; negativo
    mov edx, 4096
    ret
