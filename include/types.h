/* ============================================================================
 * types.h  -  Tipos y constantes comunes a todo el proyecto
 * Proyecto: ia32-assembler-linker (EPC 2026-2, UNAM)
 *
 * Aqui se definen los tipos base que comparten el lexer, el parser, el
 * ensamblador, el encoder y el linker. Mantener estas definiciones en un
 * solo lugar evita inconsistencias entre modulos.
 * ==========================================================================*/
#ifndef IA32_TYPES_H
#define IA32_TYPES_H

#define MAXNAME       64    /* longitud maxima de un identificador            */
#define MAX_OPERANDS  2     /* el subconjunto soportado usa a lo mas 2 oper.  */

/* ---------------------------------------------------------------------------
 * Registros de 32 bits soportados.
 *
 * IMPORTANTE: el valor numerico asignado a cada registro coincide a proposito
 * con el codigo de 3 bits que IA-32 usa en los campos reg / r/m / index / base
 * de ModRM y SIB. Asi el encoder puede usar el enum directamente.
 *
 *   0 EAX   1 ECX   2 EDX   3 EBX   4 ESP   5 EBP   6 ESI   7 EDI
 * ------------------------------------------------------------------------- */
typedef enum {
    REG_EAX = 0, REG_ECX = 1, REG_EDX = 2, REG_EBX = 3,
    REG_ESP = 4, REG_EBP = 5, REG_ESI = 6, REG_EDI = 7,
    REG_NONE = -1
} Register;

/* Secciones minimas exigidas por el documento del proyecto. */
typedef enum {
    SEC_TEXT = 0,
    SEC_DATA = 1,
    SEC_BSS  = 2,
    SEC_NONE = -1
} SectionId;
#define SEC_COUNT 3

/* Clase de un simbolo dentro de la tabla de simbolos. */
typedef enum {
    SYM_LABEL,   /* etiqueta -> direccion (offset) dentro de una seccion */
    SYM_CONST,   /* constante definida con EQU (valor absoluto)          */
    SYM_EXTERN,  /* simbolo externo: lo resuelve el linker               */
    SYM_UNDEF    /* declarado/referenciado pero aun sin definir          */
} SymKind;

/* Tipo de relocacion almacenada en el archivo objeto. */
typedef enum {
    REL_ABS32,   /* campo de 32 bits = direccion_absoluta(sym) + addend  */
    REL_REL32    /* campo de 32 bits = sym - (sitio + 4) + addend        */
} RelocKind;

/* Utilidades de nombres (implementadas en types.c). */
const char *reg_name(Register r);
Register    reg_from_name(const char *s);
const char *section_name(SectionId s);
SectionId   section_from_name(const char *s);
const char *reloc_kind_name(RelocKind k);
const char *sym_kind_name(SymKind k);

#endif /* IA32_TYPES_H */
