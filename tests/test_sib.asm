; Caso: SIB y todos los modos de direccionamiento de memoria
section .text
global _start
_start:
    mov eax, [ebx+ecx]        ; base + indice (escala 1)
    mov eax, [ebx+ecx*4]      ; base + indice escalado
    mov eax, [ebx+ecx*4+32]   ; base + indice escalado + desplazamiento
    mov eax, [ebx+esi+8]      ; base + indice + desplazamiento
    mov eax, [esi+ecx]        ; base + indice
    mov eax, [1000]           ; memoria directa
    mov eax, [ebp+4]          ; base + desplazamiento
    mov eax, [ecx*8+16]       ; indice escalado sin base
    ret
