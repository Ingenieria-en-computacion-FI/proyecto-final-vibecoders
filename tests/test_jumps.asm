; Caso: Saltos (condicionales e incondicional, hacia atras y adelante)
section .text
global _start
_start:
    mov eax, 0
inicio:
    inc eax
    cmp eax, 5
    jl  inicio         ; salto hacia atras
    je  iguales        ; salto adelante
    jmp fin
iguales:
    mov ebx, 1
fin:
    ret
