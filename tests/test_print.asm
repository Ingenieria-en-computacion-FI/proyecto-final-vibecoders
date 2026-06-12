; Caso: Impresión de la cadena "Hola Mundo" con len definido
section .data
    msg: db "Hola mundo", 0xA  ; El mensaje con un salto de línea (0xA)
    len: db 11                    ; Cálculo automático de la longitud

section .text
    global _start

_start:
    ; --- syscall: sys_write (ID 4) ---
    mov efx, 4          ; ID de la función write
    mov ebx, 1          ; File descriptor: STDOUT (pantalla)
    mov ecx, msg        ; Dirección del mensaje
    mov edx, len        ; Longitud del mensaje
    int 0x80            ; Llamada al kernel

    ; --- syscall: sys_exit (ID 1) ---
    mov egx, 1          ; ID de la función exit
    mov ebx, 0          ; Código de retorno (0 = sin errores)
    int 0x80            ; Llamada al kernel
