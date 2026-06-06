/* ============================================================================
 * symtab.c  -  Tabla de simbolos (arreglo dinamico simple)
 * ==========================================================================*/
#include <string.h>
#include <stdlib.h>
#include "symtab.h"
#include "util.h"

void symtab_init(SymTab *t) {
    t->items = NULL; t->count = 0; t->cap = 0;
}

void symtab_free(SymTab *t) {
    free(t->items);
    t->items = NULL; t->count = 0; t->cap = 0;
}

Symbol *symtab_find(SymTab *t, const char *name) {
    for (size_t i = 0; i < t->count; i++)
        if (strcmp(t->items[i].name, name) == 0)
            return &t->items[i];
    return NULL;
}

/* Crea una entrada vacia (UNDEF) para 'name' y la devuelve. */
static Symbol *symtab_new(SymTab *t, const char *name) {
    if (t->count == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 16;
        t->items = xrealloc(t->items, t->cap * sizeof(Symbol));
    }
    Symbol *s = &t->items[t->count++];
    memset(s, 0, sizeof *s);
    strncpy(s->name, name, MAXNAME - 1);
    s->kind      = SYM_UNDEF;
    s->section   = SEC_NONE;
    s->value     = 0;
    s->is_global = 0;
    s->defined   = 0;
    return s;
}

Symbol *symtab_define_label(SymTab *t, const char *name,
                            SectionId sec, long value, int *redefined) {
    if (redefined) *redefined = 0;
    Symbol *s = symtab_find(t, name);
    if (!s) s = symtab_new(t, name);

    if (s->defined) {                 /* ya tenia una definicion -> conflicto */
        if (redefined) *redefined = 1;
        return s;
    }
    s->kind    = SYM_LABEL;
    s->section = sec;
    s->value   = value;
    s->defined = 1;
    return s;
}

Symbol *symtab_define_const(SymTab *t, const char *name,
                            long value, int *redefined) {
    if (redefined) *redefined = 0;
    Symbol *s = symtab_find(t, name);
    if (!s) s = symtab_new(t, name);

    if (s->defined) {
        if (redefined) *redefined = 1;
        return s;
    }
    s->kind    = SYM_CONST;
    s->section = SEC_NONE;
    s->value   = value;
    s->defined = 1;
    return s;
}

Symbol *symtab_declare_extern(SymTab *t, const char *name) {
    Symbol *s = symtab_find(t, name);
    if (!s) s = symtab_new(t, name);
    /* Solo lo marcamos externo si aun no esta definido localmente. */
    if (!s->defined) {
        s->kind    = SYM_EXTERN;
        s->section = SEC_NONE;
    }
    return s;
}

void symtab_mark_global(SymTab *t, const char *name) {
    Symbol *s = symtab_find(t, name);
    if (!s) s = symtab_new(t, name);
    s->is_global = 1;
}
