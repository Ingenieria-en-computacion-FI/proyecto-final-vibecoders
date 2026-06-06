/* ============================================================================
 * encoder.h  -  Generador de codigo maquina IA-32 (subconjunto)
 *
 * Responsabilidad (Integrante 4):
 *   - Traducir una instruccion (Statement ST_INSTR) a bytes IA-32:
 *       [opcode] [ModRM] [SIB] [desplazamiento] [inmediato]
 *   - Construir ModRM y SIB para los modos de direccionamiento soportados.
 *
 * NOTA DE DISEÑO:
 *   El encoder es puramente sintactico: NO resuelve simbolos. Cuando un campo
 *   de 32 bits corresponde a un simbolo, devuelve una referencia (EncRef) que
 *   el backend del ensamblador resolvera (constante, rel local) o convertira
 *   en una relocacion para el linker.
 *
 *   El TAMAÑO de cada instruccion es determinista: depende solo de la "forma"
 *   de los operandos (registros, si el desplazamiento cabe en 8 bits, si hay
 *   simbolo, etc.), nunca del valor de un simbolo. Gracias a esto la 1a y la
 *   2a pasada calculan exactamente los mismos tamaños. Los saltos siempre se
 *   codifican como "near" (rel32) para conservar esta propiedad.
 * ==========================================================================*/
#ifndef IA32_ENCODER_H
#define IA32_ENCODER_H

#include "types.h"
#include "parser.h"

/* Referencia a un simbolo dentro de los bytes generados. */
typedef struct {
    int       field_off;     /* offset del campo de 32 bits en la instruccion */
    RelocKind kind;          /* ABS32 (direccion) o REL32 (relativo)          */
    char      sym[MAXNAME];  /* simbolo referenciado                          */
    long      addend;        /* constante asociada (desplazamiento literal)   */
} EncRef;

/* Resultado de codificar una instruccion. */
typedef struct {
    unsigned char bytes[24]; /* suficiente para el subconjunto soportado      */
    int           len;       /* numero de bytes generados                     */
    EncRef        refs[4];   /* referencias a simbolos                         */
    int           nrefs;
    int           ok;        /* 1 si la codificacion fue valida               */
    char          err[160];  /* mensaje de error si ok == 0                    */
} EncResult;

EncResult encode_instruction(const Statement *st);

#endif /* IA32_ENCODER_H */
