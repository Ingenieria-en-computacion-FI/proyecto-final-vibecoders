/* ============================================================================
 * asmcore.c  -  Nucleo comun del ensamblador
 *
 * assemble_stream(): recorre el fuente una vez y emite cada sentencia dentro
 *                    de un ObjModule, definiendo simbolos (si se pide) y
 *                    registrando referencias pendientes (fixups).
 * resolve_refs():    convierte las referencias pendientes en parches locales
 *                    o en relocaciones para el linker.
 *
 * Ambos ensambladores (1 y 2 pasadas) reutilizan estas funciones, lo que
 * garantiza que produzcan exactamente el mismo diseño de secciones.
 * ==========================================================================*/
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "asmcore.h"
#include "encoder.h"
#include "util.h"

/* ----------------------- listas dinamicas auxiliares -------------------- */
void reflist_init(RefList *r) { r->items = NULL; r->count = 0; r->cap = 0; }
void reflist_free(RefList *r) { free(r->items); r->items = NULL; r->count = 0; r->cap = 0; }
void reflist_add (RefList *r, const PendingRef *pr) {
    if (r->count == r->cap) {
        r->cap = r->cap ? r->cap * 2 : 16;
        r->items = xrealloc(r->items, r->cap * sizeof(PendingRef));
    }
    r->items[r->count++] = *pr;
}

void sizetab_init(SizeTable *s) { s->items = NULL; s->count = 0; s->cap = 0; }
void sizetab_free(SizeTable *s) { free(s->items); s->items = NULL; s->count = 0; s->cap = 0; }
static void sizetab_add(SizeTable *s, SectionId sec, long addr, int size,
                        int line, const char *text) {
    if (!s) return;
    if (s->count == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 32;
        s->items = xrealloc(s->items, s->cap * sizeof(SizeEntry));
    }
    SizeEntry *e = &s->items[s->count++];
    e->section = sec;
    e->address = addr;
    e->size    = size;
    e->line    = line;
    strncpy(e->text, text, sizeof e->text - 1);
    e->text[sizeof e->text - 1] = '\0';
}

static int ieq(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == *b;
}

/* Buffer de bytes de la seccion actual (NULL para .bss). */
static ByteBuf *cur_buf(ObjModule *m) {
    if (m->cur == SEC_TEXT) return &m->text;
    if (m->cur == SEC_DATA) return &m->data;
    return NULL;  /* .bss no almacena bytes */
}

/* ===========================================================================
 * assemble_stream
 * =========================================================================*/
int assemble_stream(const char *src, const char *filename,
                    ObjModule *m, SymTab *st,
                    RefList *refs, int define_labels,
                    SizeTable *sizes) {
    Parser p;
    parser_init(&p, src, filename);
    int errc = 0;

    Statement s;
    while (parser_next(&p, &s)) {

        /* --- etiqueta al inicio de la linea --- */
        if (s.has_label && define_labels) {
            Symbol *prev = symtab_find(st, s.label);
            if (prev && prev->kind == SYM_EXTERN) {
                error_at(filename, s.line,
                         "el simbolo externo '%s' no puede definirse localmente", s.label);
                errc++;
            } else {
                int redef = 0;
                symtab_define_label(st, s.label, m->cur,
                                    obj_section_len(m, m->cur), &redef);
                if (redef) {
                    error_at(filename, s.line, "redefinicion de la etiqueta '%s'", s.label);
                    errc++;
                }
            }
        }

        if (s.type == ST_LABEL_ONLY) continue;

        /* ----------------------------------------------------------------
         * Directivas
         * ---------------------------------------------------------------- */
        if (s.type == ST_DIRECTIVE) {
            const char *d = s.directive;

            if (ieq(d, "section")) {
                SectionId ns = section_from_name(s.dname);
                if (ns == SEC_NONE) {
                    error_at(filename, s.line, "seccion desconocida '%s'", s.dname);
                    errc++;
                } else {
                    m->cur = ns;
                }
            }
            else if (ieq(d, "global")) {
                if (define_labels) symtab_mark_global(st, s.dname);
            }
            else if (ieq(d, "extern")) {
                if (define_labels) symtab_declare_extern(st, s.dname);
            }
            else if (ieq(d, "org")) {
                if (define_labels && s.ndargs >= 1) m->org[m->cur] = s.dvals[0];
            }
            else if (ieq(d, "equ")) {
                if (define_labels) {
                    long val = 0; int okval = 1;
                    if (s.ndargs >= 1 && s.dsym_flag[0]) {
                        Symbol *ref = symtab_find(st, s.dsyms[0]);
                        if (ref && ref->defined) val = ref->value;
                        else { error_at(filename, s.line,
                               "EQU '%s' referencia un simbolo no definido", s.dname);
                               errc++; okval = 0; }
                    } else if (s.ndargs >= 1) {
                        val = s.dvals[0];
                    } else { okval = 0; }
                    if (okval) {
                        int redef = 0;
                        symtab_define_const(st, s.dname, val, &redef);
                        if (redef) {
                            error_at(filename, s.line, "redefinicion de '%s'", s.dname);
                            errc++;
                        }
                    }
                }
            }
            else if (ieq(d, "db") || ieq(d, "dw") || ieq(d, "dd")) {
                ByteBuf *buf = cur_buf(m);
                if (!buf) {
                    error_at(filename, s.line, "no se pueden colocar datos en .bss");
                    errc++;
                } else {
                    long before = (long)buf->len;
                    int unit = ieq(d, "db") ? 1 : (ieq(d, "dw") ? 2 : 4);
                    for (int i = 0; i < s.ndargs; i++) {
                        if (s.dsym_flag[i]) {
                            if (unit != 4) {
                                error_at(filename, s.line,
                                         "solo 'dd' admite simbolos (direcciones)");
                                errc++;
                                bb_put32(buf, 0);
                            } else {
                                if (refs) {
                                    PendingRef pr;
                                    memset(&pr, 0, sizeof pr);
                                    pr.section = m->cur;
                                    pr.offset  = (long)buf->len;
                                    pr.kind    = REL_ABS32;
                                    pr.addend  = 0;
                                    pr.line    = s.line;
                                    strncpy(pr.sym, s.dsyms[i], MAXNAME - 1);
                                    reflist_add(refs, &pr);
                                }
                                bb_put32(buf, 0);
                            }
                        } else {
                            unsigned long v = (unsigned long)s.dvals[i];
                            if (unit == 1) bb_push(buf, (unsigned char)(v & 0xFF));
                            else if (unit == 2) {
                                bb_push(buf, (unsigned char)(v & 0xFF));
                                bb_push(buf, (unsigned char)((v >> 8) & 0xFF));
                            } else bb_put32(buf, (unsigned int)v);
                        }
                    }
                    sizetab_add(sizes, m->cur, before, (int)((long)buf->len - before),
                                s.line, d);
                }
            }
            else if (ieq(d, "resb") || ieq(d, "resw") || ieq(d, "resd")) {
                if (m->cur != SEC_BSS) {
                    error_at(filename, s.line, "'%s' solo es valido en .bss", d);
                    errc++;
                } else {
                    long unit  = ieq(d, "resb") ? 1 : (ieq(d, "resw") ? 2 : 4);
                    long count = (s.ndargs >= 1) ? s.dvals[0] : 0;
                    long before = m->bss_size;
                    m->bss_size += unit * count;
                    sizetab_add(sizes, SEC_BSS, before, (int)(m->bss_size - before),
                                s.line, d);
                }
            }
            continue;
        }

        /* ----------------------------------------------------------------
         * Instrucciones
         * ---------------------------------------------------------------- */
        if (s.type == ST_INSTR) {
            ByteBuf *buf = cur_buf(m);
            if (!buf) {
                error_at(filename, s.line, "no se puede emitir codigo en .bss");
                errc++;
                continue;
            }
            EncResult e = encode_instruction(&s);
            if (!e.ok) {
                error_at(filename, s.line, "%s", e.err);
                errc++;
                continue;
            }
            long start = (long)buf->len;
            bb_append(buf, e.bytes, (size_t)e.len);

            if (refs) {
                for (int i = 0; i < e.nrefs; i++) {
                    PendingRef pr;
                    memset(&pr, 0, sizeof pr);
                    pr.section = m->cur;
                    pr.offset  = start + e.refs[i].field_off;
                    pr.kind    = e.refs[i].kind;
                    pr.addend  = e.refs[i].addend;
                    pr.line    = s.line;
                    strncpy(pr.sym, e.refs[i].sym, MAXNAME - 1);
                    reflist_add(refs, &pr);
                }
            }
            sizetab_add(sizes, m->cur, start, e.len, s.line, s.mnemonic);
        }
    }

    errc += p.error_count;
    return errc;
}

/* ===========================================================================
 * resolve_refs
 * =========================================================================*/
static ByteBuf *section_buf(ObjModule *m, SectionId s) {
    if (s == SEC_TEXT) return &m->text;
    if (s == SEC_DATA) return &m->data;
    return NULL;
}

int resolve_refs(ObjModule *m, SymTab *st, RefList *refs) {
    int errc = 0;

    for (size_t i = 0; i < refs->count; i++) {
        PendingRef *p = &refs->items[i];
        Symbol *s = symtab_find(st, p->sym);
        ByteBuf *buf = section_buf(m, p->section);

        if (!s) {
            error_at("(resolucion)", p->line, "simbolo indefinido '%s'", p->sym);
            errc++; continue;
        }

        if (s->kind == SYM_CONST) {
            long val = s->value + p->addend;
            if (buf) {
                if (p->kind == REL_REL32)
                    bb_patch32(buf, (size_t)p->offset, (unsigned int)(val - (p->offset + 4)));
                else
                    bb_patch32(buf, (size_t)p->offset, (unsigned int)val);
            }
        }
        else if (s->kind == SYM_LABEL && s->defined) {
            if (p->kind == REL_REL32) {
                if (s->section == p->section) {
                    /* relativo dentro de la misma seccion: se resuelve aqui */
                    long rel = s->value - (p->offset + 4) + p->addend;
                    if (buf) bb_patch32(buf, (size_t)p->offset, (unsigned int)rel);
                } else {
                    /* relativo entre secciones: lo resuelve el linker */
                    obj_add_reloc(m, p->section, p->offset, p->sym, REL_REL32, p->addend);
                }
            } else {
                /* direccion absoluta: siempre relocacion (la fija el linker) */
                obj_add_reloc(m, p->section, p->offset, p->sym, REL_ABS32, p->addend);
            }
        }
        else if (s->kind == SYM_EXTERN) {
            obj_add_reloc(m, p->section, p->offset, p->sym, p->kind, p->addend);
        }
        else {
            /* UNDEF: declarado global pero nunca definido, o solo referenciado */
            error_at("(resolucion)", p->line,
                     "simbolo '%s' referenciado pero no definido%s",
                     p->sym, s->is_global ? " (declarado global)" : "");
            errc++;
        }
    }
    return errc;
}
