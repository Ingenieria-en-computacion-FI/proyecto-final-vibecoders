/* ============================================================================
 * asmcore.h  -  Nucleo comun del ensamblador (backend compartido)
 *
 * Responsabilidad (Lizeth):
 *   Logica compartida por el ensamblador de 1 pasada y el de 2 pasadas:
 *     - emitir una sentencia (directiva o instruccion) dentro de un modulo,
 *     - registrar referencias pendientes (fixups) a simbolos,
 *     - resolver esas referencias al final: parchar valores locales/constantes
 *       o generar relocaciones para el linker.
 *
 * ==========================================================================*/
#ifndef IA32_ASMCORE_H
#define IA32_ASMCORE_H

#include <stddef.h>
#include "types.h"
#include "parser.h"
#include "symtab.h"
#include "objfile.h"


typedef struct {
    SectionId section;       /* seccion donde vive el campo de 32 bits        */
    long      offset;        /* offset del campo dentro de la seccion         */
    RelocKind kind;        
    char      sym[MAXNAME];
    long      addend;
    int       line;          /* linea fuente (para la tabla de referencias)   */
} PendingRef;

typedef struct {
    PendingRef *items;
    size_t      count, cap;
} RefList;

void reflist_init(RefList *r);
void reflist_free(RefList *r);
void reflist_add (RefList *r, const PendingRef *pr);

/* Entrada de la tabla de tamaños (1a pasada del ensamblador de 2 pasadas). */
typedef struct {
    SectionId section;
    long      address;       /* offset (relativo a la seccion) de la sentencia*/
    int       size;          /* bytes que ocupa                               */
    int       line;
    char      text[48];      /* mnemonico o directiva (descripcion corta)     */
} SizeEntry;

typedef struct {
    SizeEntry *items;
    size_t     count, cap;
} SizeTable;

void sizetab_init(SizeTable *s);
void sizetab_free(SizeTable *s);

/* ---------------------------------------------------------------------------
 * Recorre TODO el fuente una vez, emitiendo cada sentencia en 'm'.
 *
 *   define_labels : 1 -> define etiquetas/constantes/extern/global
 *                        (usar en 1a pasada y en el ensamblador de 1 pasada)
 *                   0 -> no toca la tabla de simbolos, solo emite
 *                        (usar en la 2a pasada)
 *   refs   : recibe las referencias pendientes generadas (puede ser NULL).
 *   sizes  : recibe la tabla de tamaños (puede ser NULL).
 *
 * Devuelve el numero de errores encontrados.
 * ------------------------------------------------------------------------- */
int assemble_stream(const char *src, const char *filename,
                    ObjModule *m, SymTab *st,
                    RefList *refs, int define_labels,
                    SizeTable *sizes);

/* ---------------------------------------------------------------------------
 * Resuelve las referencias pendientes usando la tabla de simbolos ya completa:
 *   - constante              -> parcha el campo con el valor.
 *   - etiqueta local + REL32 misma seccion -> parcha el desplazamiento relativo.
 *   - etiqueta local + ABS32 -> genera relocacion ABS32.
 *   - cruce de seccion / EXTERN -> genera relocacion.
 *   - simbolo indefinido y no externo -> error.
 *
 * Devuelve el numero de errores (p. ej. simbolos indefinidos).
 * ------------------------------------------------------------------------- */
int resolve_refs(ObjModule *m, SymTab *st, RefList *refs);

#endif 
