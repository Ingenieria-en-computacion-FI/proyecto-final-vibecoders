/* ============================================================================
 * assembler1.c  -  Ensamblador de UNA pasada
 *
 * Uso:  ./assembler1 entrada.asm  prefijo_salida
 *
 * Lee el archivo una sola vez, define simbolos a medida que aparecen, genera
 * codigo (parcial para las referencias adelantadas) y al final aplica los
 * fixups: parcha lo que se puede resolver localmente y convierte el resto en
 * relocaciones.
 *
 * Genera:
 *   prefijo.o                 archivo objeto (con codigo en hex)
 *   prefijo_symbols.txt       tabla de simbolos
 *   prefijo_references.txt    tabla de referencias (fixups)
 *   prefijo_sections.txt      tabla de secciones
 *   prefijo_relocations.txt   tabla de relocaciones
 * ==========================================================================*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "asmcore.h"
#include "encoder.h"
#include "objfile.h"
#include "util.h"

static const char *base_name(const char *path) {
    const char *b = path;
    for (const char *p = path; *p; p++)
        if (*p == '/' || *p == '\\') b = p + 1;
    return b;
}

/* Vuelca la tabla de referencias (fixups) con su estado de resolucion. */
static void dump_references(const char *path, SymTab *st, RefList *refs) {
    FILE *f = fopen(path, "w");
    if (!f) { error_at(path, 0, "no se pudo crear la tabla de referencias"); return; }
    fprintf(f, "# Tabla de referencias (fixups detectados en la unica pasada)\n");
    fprintf(f, "# %-6s %-8s %-18s %-7s %-7s %s\n",
            "LINEA", "SECCION", "SIMBOLO", "TIPO", "ADDEND", "ESTADO");
    for (size_t i = 0; i < refs->count; i++) {
        PendingRef *p = &refs->items[i];
        Symbol *s = symtab_find(st, p->sym);
        const char *estado;
        if (!s)                              estado = "INDEFINIDO";
        else if (s->kind == SYM_CONST)       estado = "constante";
        else if (s->kind == SYM_EXTERN)      estado = "reloc(extern)";
        else if (s->kind == SYM_LABEL && s->defined) {
            if (p->kind == REL_REL32 && s->section == p->section)
                estado = "resuelta(local)";
            else if (p->kind == REL_REL32)
                estado = "reloc(rel/seccion)";
            else
                estado = "reloc(abs)";
        } else                               estado = "pendiente";
        fprintf(f, "  %-6d %-8s %-18s %-7s %-7ld %s\n",
                p->line, section_name(p->section), p->sym,
                reloc_kind_name(p->kind), p->addend, estado);
    }
    if (refs->count == 0) fprintf(f, "# (sin referencias)\n");
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "uso: %s entrada.asm prefijo_salida\n", argv[0]);
        return 2;
    }
    const char *input  = argv[1];
    const char *prefix = argv[2];

    char *src = read_file(input);

    SymTab st;   symtab_init(&st);
    ObjModule m; obj_module_init(&m, &st);
    strncpy(m.srcname, base_name(input), sizeof m.srcname - 1);

    RefList refs; reflist_init(&refs);

    /* UNICA pasada: define simbolos y emite codigo, registrando fixups. */
    int errc = assemble_stream(src, input, &m, &st, &refs,
                               /*define_labels=*/1, /*sizes=*/NULL);

    /* Fase de fixups: parcha locales y genera relocaciones. */
    errc += resolve_refs(&m, &st, &refs);

    /* Rutas de salida. */
    char po[512], psym[512], pref[512], psec[512], prel[512];
    snprintf(po,   sizeof po,   "%s.o",                prefix);
    snprintf(psym, sizeof psym, "%s_symbols.txt",      prefix);
    snprintf(pref, sizeof pref, "%s_references.txt",   prefix);
    snprintf(psec, sizeof psec, "%s_sections.txt",     prefix);
    snprintf(prel, sizeof prel, "%s_relocations.txt",  prefix);

    obj_write(po, &m);
    dump_symbols(psym, &st);
    dump_references(pref, &st, &refs);
    dump_sections(psec, &m);
    dump_relocations(prel, &m);

    printf("== assembler1 (una pasada) ==\n");
    printf("  fuente   : %s\n", input);
    printf("  .text    : %zu bytes\n", m.text.len);
    printf("  .data    : %zu bytes\n", m.data.len);
    printf("  .bss     : %ld bytes\n", m.bss_size);
    printf("  simbolos : %zu\n", st.count);
    printf("  reubic.  : %zu\n", m.nreloc);
    printf("  salida   : %s, %s, %s, %s, %s\n", po, psym, pref, psec, prel);
    if (errc) printf("  ATENCION : se encontraron %d error(es); revise los mensajes.\n", errc);

    reflist_free(&refs);
    obj_module_free(&m);
    symtab_free(&st);
    free(src);
    return errc ? 1 : 0;
}
