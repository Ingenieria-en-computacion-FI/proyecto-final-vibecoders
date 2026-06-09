; ============================================================
; Define 'helper' (global) y referencia 'main' (extern).
; ============================================================

section .data
tabla:  dd 7, 8, 9

section .text
global helper
extern main

helper:
    mov ecx, [tabla]    ; ABS32 a dato local -> relocacion
    mov edx, main       ; direccion de 'main' (EXTERN) -> ABS32 relocacion
    ret
