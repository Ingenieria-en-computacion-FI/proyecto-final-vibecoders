; ============================================================
; Modulo principal (para el linker)
; Define 'main' (global) y usa 'helper' (extern, en mod2).
; ============================================================

section .data
valor:  dd 42

section .text
global main
extern helper

main:
    mov eax, [valor]    ; ABS32 a dato local (otra seccion) -> relocacion
    push eax
    call helper         ; REL32 a simbolo EXTERN          -> relocacion
    add  eax, ecx
    ret
