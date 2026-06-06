; ============================================================
; examples/test1.asm
; Programa de demostracion general del ensamblador.
; Ejercita: inmediato, registro-registro, memoria directa,
; base+desplazamiento, indice escalado (SIB), datos, .bss,
; referencia adelantada (forward) y saltos.
; ============================================================

section .data
mensaje:    db "Hola", 0          ; cadena terminada en cero
numero:     dd 12345              ; entero de 32 bits
arreglo:    dd 1, 2, 3, 4         ; cuatro enteros

section .bss
buffer:     resb 16               ; 16 bytes sin inicializar
contador:   resd 1                ; un entero reservado

section .text
global _start

_start:
    mov eax, 10                   ; inmediato
    mov ebx, eax                  ; registro a registro
    mov ecx, [numero]             ; memoria directa (simbolo) -> ABS32
    mov edx, [ebp+4]              ; base + desplazamiento
    lea esi, [arreglo]            ; direccion de un dato -> ABS32
    mov edi, [esi+ecx*4]          ; base + indice escalado (SIB)
    add eax, ebx
    cmp eax, 100
    jl  menor                     ; referencia adelantada
    mov eax, 0
    jmp fin
menor:
    mov eax, 1
fin:
    ret
