/* ============================================================================
 * parser.h  -  Analizador sintactico MANUAL (sin yacc/bison)
 *
 * Responsabilidad (Integrante 2):
 *   - Consumir los tokens del lexer y construir una representacion interna
 *     "Statement" por cada linea: etiqueta, directiva o instruccion.
 *   - Parsear operandos y modos de direccionamiento (registro, inmediato,
 *     memoria directa, base+desplazamiento, base+indice, base+indice*escala,
 *     base+indice*escala+desplazamiento).
 * ==========================================================================*/
#ifndef IA32_PARSER_H
#define IA32_PARSER_H

#include "types.h"
#include "lexer.h"

/* Clase de operando. */
typedef enum {
    OPND_NONE,
    OPND_REG,    /* un registro                                              */
    OPND_IMM,    /* inmediato (valor) o simbolo usado como direccion         */
    OPND_MEM     /* referencia a memoria [ ... ]                             */
} OperandKind;

/* Un operando completo. Para OPND_MEM se usan base/index/scale/disp; un
   simbolo dentro de corchetes (p. ej. [msg+4]) se guarda en sym/has_sym. */
typedef struct {
    OperandKind kind;

    Register reg;            /* OPND_REG                                      */

    long     imm;            /* OPND_IMM: valor inmediato                     */

    Register base;           /* OPND_MEM: registro base  (o REG_NONE)         */
    Register index;          /* OPND_MEM: registro indice (o REG_NONE)        */
    int      scale;          /* OPND_MEM: 1,2,4,8                             */
    long     disp;           /* OPND_MEM: desplazamiento literal              */

    char     sym[MAXNAME];   /* simbolo referenciado ("" si ninguno)          */
    int      has_sym;        /* 1 si el operando referencia un simbolo        */
} Operand;

/* Tipo de sentencia (una por linea logica). */
typedef enum {
    ST_NONE,
    ST_INSTR,       /* instruccion con 0..2 operandos                        */
    ST_DIRECTIVE,   /* directiva (section, db, equ, ...)                     */
    ST_LABEL_ONLY   /* una etiqueta sola en su linea                         */
} StmtType;

typedef struct {
    StmtType type;
    int      line;

    /* Etiqueta opcional definida al inicio de la linea (label:). */
    char     label[MAXNAME];
    int      has_label;

    /* --- instruccion --- */
    char     mnemonic[MAXNAME];
    Operand  ops[MAX_OPERANDS];
    int      nops;

    /* --- directiva --- */
    char     directive[MAXNAME];      /* section/global/extern/db/dw/dd/...   */
    char     dname[MAXNAME];          /* nombre asociado (section/global/...) */

    /* argumentos de datos (db/dw/dd) o valor (resb/resw/resd/equ/org)        */
    long     dvals[64];
    char     dsyms[64][MAXNAME];
    int      dsym_flag[64];           /* 1 si el arg i es un simbolo          */
    int      ndargs;

    char     dstr[256];               /* cadena para  db "texto"              */
    int      dstr_len;
    int      has_dstr;
} Statement;

typedef struct {
    Lexer       lx;
    Token       cur;          /* token de lookahead                          */
    const char *filename;
    int         error_count;
} Parser;

void parser_init(Parser *p, const char *src, const char *filename);

/* Obtiene la siguiente sentencia. Devuelve 1 si produjo una, 0 al EOF.
   Las sentencias vacias (lineas en blanco/comentarios) se omiten. */
int  parser_next(Parser *p, Statement *out);

#endif /* IA32_PARSER_H */
