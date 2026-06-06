/* ============================================================================
 * objfile.c  -  Escritura/lectura del archivo objeto y volcado de tablas
 *
 * El archivo objeto es de TEXTO, con una gramatica simple basada en tokens
 * (ver docs/formato_objeto.md). Esto facilita inspeccionarlo y depurarlo.
 * ==========================================================================*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "objfile.h"
#include "util.h"

/* ---------------- Modulo en construccion (escritura) ------------------- */
void obj_module_init(ObjModule *m, SymTab *st) {
    memset(m, 0, sizeof *m);
    bb_init(&m->text);
    bb_init(&m->data);
    m->bss_size  = 0;
    m->cur       = SEC_TEXT;
    m->symtab    = st;
    m->relocs    = NULL;
    m->nreloc    = 0;
    m->reloc_cap = 0;
    for (int i = 0; i < SEC_COUNT; i++) m->org[i] = 0;
    m->srcname[0] = '\0';
}

void obj_module_free(ObjModule *m) {
    bb_free(&m->text);
    bb_free(&m->data);
    free(m->relocs);
    m->relocs = NULL; m->nreloc = 0; m->reloc_cap = 0;
}

void obj_add_reloc(ObjModule *m, SectionId sec, long off,
                   const char *sym, RelocKind kind, long addend) {
    if (m->nreloc == m->reloc_cap) {
        m->reloc_cap = m->reloc_cap ? m->reloc_cap * 2 : 16;
        m->relocs = xrealloc(m->relocs, m->reloc_cap * sizeof(Reloc));
    }
    Reloc *r = &m->relocs[m->nreloc++];
    r->section = sec;
    r->offset  = off;
    r->kind    = kind;
    r->addend  = addend;
    strncpy(r->sym, sym, MAXNAME - 1);
    r->sym[MAXNAME - 1] = '\0';
}

long obj_section_len(const ObjModule *m, SectionId s) {
    switch (s) {
        case SEC_TEXT: return (long)m->text.len;
        case SEC_DATA: return (long)m->data.len;
        case SEC_BSS:  return m->bss_size;
        default:       return 0;
    }
}

static void write_hex_block(FILE *f, const char *secname,
                            const unsigned char *data, size_t n) {
    fprintf(f, "CODE %s %zu\n", secname, n);
    for (size_t i = 0; i < n; i++) {
        fprintf(f, "%02X", data[i]);
        if ((i % 16) == 15 || i + 1 == n) fprintf(f, "\n");
        else                              fprintf(f, " ");
    }
    if (n == 0) fprintf(f, "\n");
}

int obj_write(const char *path, ObjModule *m) {
    FILE *f = fopen(path, "w");
    if (!f) { error_at(path, 0, "no se pudo crear el archivo objeto"); return 1; }

    fprintf(f, "IA32OBJ 1\n");
    fprintf(f, "SRC %s\n", m->srcname[0] ? m->srcname : "modulo");

    fprintf(f, "SECTION .text %zu\n", m->text.len);
    fprintf(f, "SECTION .data %zu\n", m->data.len);
    fprintf(f, "SECTION .bss %ld\n",  m->bss_size);

    /* Tabla de simbolos. */
    SymTab *t = m->symtab;
    for (size_t i = 0; i < t->count; i++) {
        Symbol *s = &t->items[i];
        const char *type = NULL, *sec = "-";
        if (s->kind == SYM_LABEL && s->defined) {
            type = s->is_global ? "GLOBAL" : "LOCAL";
            sec  = section_name(s->section);
        } else if (s->kind == SYM_CONST) {
            type = "CONST";
        } else if (s->kind == SYM_EXTERN) {
            type = "EXTERN";
        } else {
            continue;   /* simbolos indefinidos no se serializan */
        }
        fprintf(f, "SYMBOL %s %s %s %ld\n", s->name, type, sec, s->value);
    }

    /* Tabla de relocaciones. */
    for (size_t i = 0; i < m->nreloc; i++) {
        Reloc *r = &m->relocs[i];
        fprintf(f, "RELOC %s %ld %s %s %ld\n",
                section_name(r->section), r->offset, r->sym,
                reloc_kind_name(r->kind), r->addend);
    }

    /* Codigo. */
    write_hex_block(f, ".text", m->text.data, m->text.len);
    write_hex_block(f, ".data", m->data.data, m->data.len);

    fprintf(f, "END\n");
    fclose(f);
    return 0;
}

/* ---------------- Modulo leido desde disco (lectura) ------------------- */
static void of_add_sym(ObjFile *o, const Symbol *s) {
    if (o->nsyms == o->sym_cap) {
        o->sym_cap = o->sym_cap ? o->sym_cap * 2 : 16;
        o->syms = xrealloc(o->syms, o->sym_cap * sizeof(Symbol));
    }
    o->syms[o->nsyms++] = *s;
}
static void of_add_reloc(ObjFile *o, const Reloc *r) {
    if (o->nreloc == o->reloc_cap) {
        o->reloc_cap = o->reloc_cap ? o->reloc_cap * 2 : 16;
        o->relocs = xrealloc(o->relocs, o->reloc_cap * sizeof(Reloc));
    }
    o->relocs[o->nreloc++] = *r;
}

int obj_read(const char *path, ObjFile *o) {
    FILE *f = fopen(path, "rb");
    if (!f) { error_at(path, 0, "no se pudo abrir el objeto"); return 1; }

    memset(o, 0, sizeof *o);
    for (int i = 0; i < SEC_COUNT; i++) { o->sec_size[i] = 0; bb_init(&o->sec_bytes[i]); }
    strncpy(o->name, path, sizeof o->name - 1);

    char tok[128];
    while (fscanf(f, "%127s", tok) == 1) {
        if (strcmp(tok, "IA32OBJ") == 0) {
            int ver; if (fscanf(f, "%d", &ver) != 1) ver = 0; (void)ver;
        } else if (strcmp(tok, "SRC") == 0) {
            if (fscanf(f, "%255s", o->name) != 1) o->name[0] = '\0';
        } else if (strcmp(tok, "SECTION") == 0) {
            char sn[32]; long sz;
            if (fscanf(f, "%31s %ld", sn, &sz) == 2) {
                SectionId s = section_from_name(sn);
                if (s != SEC_NONE) o->sec_size[s] = sz;
            }
        } else if (strcmp(tok, "SYMBOL") == 0) {
            char name[MAXNAME], type[16], sn[16]; long val;
            if (fscanf(f, "%63s %15s %15s %ld", name, type, sn, &val) == 4) {
                Symbol s; memset(&s, 0, sizeof s);
                strncpy(s.name, name, MAXNAME - 1);
                s.value   = val;
                s.section = section_from_name(sn);
                if (strcmp(type, "GLOBAL") == 0)      { s.kind = SYM_LABEL;  s.is_global = 1; s.defined = 1; }
                else if (strcmp(type, "LOCAL") == 0)  { s.kind = SYM_LABEL;  s.is_global = 0; s.defined = 1; }
                else if (strcmp(type, "EXTERN") == 0) { s.kind = SYM_EXTERN; s.defined = 0; }
                else if (strcmp(type, "CONST") == 0)  { s.kind = SYM_CONST;  s.defined = 1; s.section = SEC_NONE; }
                else                                  { s.kind = SYM_UNDEF; }
                of_add_sym(o, &s);
            }
        } else if (strcmp(tok, "RELOC") == 0) {
            char sn[16], sym[MAXNAME], kind[16]; long off, add;
            if (fscanf(f, "%15s %ld %63s %15s %ld", sn, &off, sym, kind, &add) == 5) {
                Reloc r; memset(&r, 0, sizeof r);
                r.section = section_from_name(sn);
                r.offset  = off;
                strncpy(r.sym, sym, MAXNAME - 1);
                r.kind    = (strcmp(kind, "REL32") == 0) ? REL_REL32 : REL_ABS32;
                r.addend  = add;
                of_add_reloc(o, &r);
            }
        } else if (strcmp(tok, "CODE") == 0) {
            char sn[16]; long n;
            if (fscanf(f, "%15s %ld", sn, &n) == 2) {
                SectionId s = section_from_name(sn);
                for (long i = 0; i < n; i++) {
                    unsigned int byte;
                    if (fscanf(f, "%x", &byte) != 1) break;
                    if (s != SEC_NONE) bb_push(&o->sec_bytes[s], (unsigned char)byte);
                }
            }
        } else if (strcmp(tok, "END") == 0) {
            break;
        }
        /* tokens desconocidos: se ignoran (robustez) */
    }
    fclose(f);
    return 0;
}

void objfile_free(ObjFile *o) {
    for (int i = 0; i < SEC_COUNT; i++) bb_free(&o->sec_bytes[i]);
    free(o->syms);   o->syms = NULL;   o->nsyms = 0;  o->sym_cap = 0;
    free(o->relocs); o->relocs = NULL; o->nreloc = 0; o->reloc_cap = 0;
}

/* ---------------- Volcado de tablas .txt -------------------------------- */
void dump_symbols(const char *path, SymTab *t) {
    FILE *f = fopen(path, "w");
    if (!f) { error_at(path, 0, "no se pudo crear la tabla de simbolos"); return; }
    fprintf(f, "# Tabla de simbolos\n");
    fprintf(f, "# %-20s %-8s %-8s %-10s %s\n", "NOMBRE", "TIPO", "SECCION", "VALOR", "DEFINIDO");
    for (size_t i = 0; i < t->count; i++) {
        Symbol *s = &t->items[i];
        fprintf(f, "%-22s %-8s %-8s 0x%08lx %s\n",
                s->name,
                sym_kind_name(s->kind),
                (s->kind == SYM_LABEL) ? section_name(s->section) : "-",
                s->value,
                s->defined ? (s->is_global ? "si(global)" : "si") : "no");
    }
    fclose(f);
}

void dump_sections(const char *path, const ObjModule *m) {
    FILE *f = fopen(path, "w");
    if (!f) { error_at(path, 0, "no se pudo crear la tabla de secciones"); return; }
    fprintf(f, "# Tabla de secciones\n");
    fprintf(f, "# %-8s %-12s %-12s\n", "SECCION", "TAMANO(hex)", "TAMANO(dec)");
    fprintf(f, "%-10s 0x%08zx %zu\n", ".text", m->text.len, m->text.len);
    fprintf(f, "%-10s 0x%08zx %zu\n", ".data", m->data.len, m->data.len);
    fprintf(f, "%-10s 0x%08lx %ld\n", ".bss",  m->bss_size, m->bss_size);
    fclose(f);
}

void dump_relocations(const char *path, const ObjModule *m) {
    FILE *f = fopen(path, "w");
    if (!f) { error_at(path, 0, "no se pudo crear la tabla de relocaciones"); return; }
    fprintf(f, "# Tabla de relocaciones\n");
    fprintf(f, "# %-8s %-10s %-20s %-7s %s\n", "SECCION", "OFFSET", "SIMBOLO", "TIPO", "ADDEND");
    for (size_t i = 0; i < m->nreloc; i++) {
        const Reloc *r = &m->relocs[i];
        fprintf(f, "%-10s 0x%08lx %-20s %-7s %ld\n",
                section_name(r->section), r->offset, r->sym,
                reloc_kind_name(r->kind), r->addend);
    }
    if (m->nreloc == 0) fprintf(f, "# (sin relocaciones)\n");
    fclose(f);
}
