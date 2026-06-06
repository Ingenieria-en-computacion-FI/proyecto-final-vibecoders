/* ============================================================================
 * assembler2.c  -  Ensamblador de DOS pasadas
 *
 * Uso:  ./assembler2 entrada.asm  prefijo_salida
 *
 * Primera pasada : construye la tabla de simbolos, calcula direcciones y
 *                  detecta redefiniciones. Genera symbols.txt y sizes.txt.
 * Segunda pasada : genera el codigo definitivo, resuelve referencias y emite
 *                  el archivo objeto, sections.txt y relocations.txt.
 *
 * Como ambas pasadas usan el mismo nucleo (assemble_stream), los tamaños que
 * calcula la primera coinciden exactamente con el codigo de la segunda.
 * ==========================================================================*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "asmcore.h"
#include "objfile.h"
#include "util.h"

static const char *base_name(const char *path) {
    const char *b = path;
    for (const char *p = path; *p; p++)
        if (*p == '/' || *p == '\\') b = p + 1;
    return b;
}

static void dump_sizes(const char *path, SizeTable *s) {
    FILE *f = fopen(path, "w");
    if (!f) { error_at(path, 0, "no se pudo crear la tabla de tamaños"); return; }
    fprintf(f, "# Tabla de tamaños de instrucciones (primera pasada)\n");
    fprintf(f, "# %-6s %-8s %-12s %-6s %s\n",
            "LINEA", "SECCION", "DIRECCION", "BYTES", "DESCRIPCION");
    for (size_t i = 0; i < s->count; i++) {
        SizeEntry *e = &s->items[i];
        fprintf(f, "  %-6d %-8s 0x%08lx %-6d %s\n",
                e->line, section_name(e->section), e->address, e->size, e->text);
    }
    if (s->count == 0) fprintf(f, "# (sin instrucciones)\n");
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

    SymTab st; symtab_init(&st);

    char po[512], psym[512], psz[512], psec[512], prel[512];
    snprintf(po,   sizeof po,   "%s.o",               prefix);
    snprintf(psym, sizeof psym, "%s_symbols.txt",     prefix);
    snprintf(psz,  sizeof psz,  "%s_sizes.txt",       prefix);
    snprintf(psec, sizeof psec, "%s_sections.txt",    prefix);
    snprintf(prel, sizeof prel, "%s_relocations.txt", prefix);

    /* ---------------- PRIMERA PASADA ---------------- */
    ObjModule m1; obj_module_init(&m1, &st);
    SizeTable sizes; sizetab_init(&sizes);
    int errc1 = assemble_stream(src, input, &m1, &st, /*refs=*/NULL,
                                /*define_labels=*/1, &sizes);
    dump_symbols(psym, &st);
    dump_sizes(psz, &sizes);
    sizetab_free(&sizes);
    obj_module_free(&m1);

    /* ---------------- SEGUNDA PASADA ---------------- */
    ObjModule m2; obj_module_init(&m2, &st);
    strncpy(m2.srcname, base_name(input), sizeof m2.srcname - 1);
    RefList refs; reflist_init(&refs);
    int errc2 = assemble_stream(src, input, &m2, &st, &refs,
                                /*define_labels=*/0, /*sizes=*/NULL);
    int errc3 = resolve_refs(&m2, &st, &refs);

    obj_write(po, &m2);
    dump_sections(psec, &m2);
    dump_relocations(prel, &m2);

    int errc = errc1 + errc2 + errc3;

    printf("== assembler2 (dos pasadas) ==\n");
    printf("  fuente   : %s\n", input);
    printf("  pasada 1 : %s, %s\n", psym, psz);
    printf("  pasada 2 : %s, %s, %s\n", po, psec, prel);
    printf("  .text    : %zu bytes\n", m2.text.len);
    printf("  .data    : %zu bytes\n", m2.data.len);
    printf("  .bss     : %ld bytes\n", m2.bss_size);
    printf("  simbolos : %zu\n", st.count);
    printf("  reubic.  : %zu\n", m2.nreloc);
    if (errc) printf("  ATENCION : se encontraron %d error(es); revise los mensajes.\n", errc);

    reflist_free(&refs);
    obj_module_free(&m2);
    symtab_free(&st);
    free(src);
    return errc ? 1 : 0;
}
