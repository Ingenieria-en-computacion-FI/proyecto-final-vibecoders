/* ============================================================================
 * symtab.h  -  Tabla de simbolos
 *
 * Guarda etiquetas (direcciones), constantes EQU, simbolos GLOBAL y EXTERN.
 * Es usada por ambos ensambladores y, en su forma serializada, por el linker.
 * ==========================================================================*/
#ifndef IA32_SYMTAB_H
#define IA32_SYMTAB_H

#include <stddef.h>
#include "types.h"

typedef struct {
    char      name[MAXNAME];
    SymKind   kind;       /* LABEL / CONST / EXTERN / UNDEF                    */
    SectionId section;    /* seccion donde vive (solo LABEL)                  */
    long      value;      /* offset dentro de la seccion (LABEL) o valor(CONST)*/
    int       is_global;  /* 1 si fue declarado GLOBAL (visible al linker)    */
    int       defined;    /* 1 si ya tiene definicion en este modulo          */
} Symbol;

typedef struct {
    Symbol *items;
    size_t  count;
    size_t  cap;
} SymTab;

void    symtab_init(SymTab *t);
void    symtab_free(SymTab *t);

Symbol *symtab_find(SymTab *t, const char *name);

/* Define una etiqueta en (sec, value). Si ya estaba definida marca
   *redefined = 1 y NO la sobreescribe (el llamador reporta el error). */
Symbol *symtab_define_label (SymTab *t, const char *name,
                             SectionId sec, long value, int *redefined);

/* Define una constante EQU. *redefined = 1 si ya existia definida. */
Symbol *symtab_define_const (SymTab *t, const char *name,
                             long value, int *redefined);

/* Declara (o asegura) un simbolo EXTERN. */
Symbol *symtab_declare_extern(SymTab *t, const char *name);

/* Marca un nombre como GLOBAL (crea entrada UNDEF si no existe). */
void    symtab_mark_global  (SymTab *t, const char *name);

#endif /* IA32_SYMTAB_H */
