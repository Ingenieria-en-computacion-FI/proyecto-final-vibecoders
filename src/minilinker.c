/* ============================================================================
 * minilinker.c  -  Mini enlazador (linker)
 *
 * Uso:  ./minilinker mod1.o mod2.o [...]  salida.hex
 *       (el ultimo argumento es siempre el archivo de salida)
 *
 * Lee N archivos objeto -con sus tablas de simbolos, relocaciones y secciones-,
 * fusiona todas las .text, todas las .data y todas las .bss, resuelve los
 * simbolos externos, reubica las direcciones y genera el binario final en
 * hexadecimal. Tambien escribe un mapa de simbolos (<salida>_map.txt).
 * ==========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "objfile.h"
#include "util.h"

#define TEXT_BASE 0x00000000UL    /* direccion base de .text (configurable)   */

static long align_up(long x, long a) { return (x + a - 1) / a * a; }

/* Entrada del mapa global de simbolos GLOBAL. */
typedef struct {
    char name[MAXNAME];
    int  module;          /* modulo que lo define                            */
    SectionId section;
    long value;           /* offset dentro de su seccion                     */
    int  is_const;
} GlobalSym;

/* Resuelve un simbolo visto desde el modulo 'mod' a su direccion final.
 * Busca primero entre los simbolos definidos localmente en ese modulo y,
 * si no lo encuentra, en el mapa global. Devuelve 1 y deja la direccion en
 * *out_addr si lo resuelve; 0 en caso contrario. */
static int resolve_symbol(ObjFile *objs, long (*mbase)[SEC_COUNT],
                          GlobalSym *globs, size_t nglob,
                          int mod, const char *want, long *out_addr) {
    for (size_t i = 0; i < objs[mod].nsyms; i++) {
        Symbol *s = &objs[mod].syms[i];
        if (strcmp(s->name, want) == 0 && s->defined && s->kind != SYM_EXTERN) {
            *out_addr = (s->kind == SYM_CONST) ? s->value
                                               : mbase[mod][s->section] + s->value;
            return 1;
        }
    }
    for (size_t k = 0; k < nglob; k++)
        if (strcmp(globs[k].name, want) == 0) {
            *out_addr = globs[k].is_const ? globs[k].value
                        : mbase[globs[k].module][globs[k].section] + globs[k].value;
            return 1;
        }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "uso: %s mod1.o [mod2.o ...] salida.hex\n", argv[0]);
        return 2;
    }

    int M = argc - 2;                  /* numero de modulos de entrada         */
    const char *outpath = argv[argc - 1];

    ObjFile *objs = xcalloc((size_t)M, sizeof(ObjFile));
    for (int j = 0; j < M; j++) {
        if (obj_read(argv[1 + j], &objs[j]) != 0)
            die("no se pudo leer el objeto '%s'", argv[1 + j]);
    }

    /* --- tamaños totales por seccion y bases globales --- */
    long total[SEC_COUNT] = {0, 0, 0};
    for (int j = 0; j < M; j++)
        for (int s = 0; s < SEC_COUNT; s++)
            total[s] += objs[j].sec_size[s];

    long gbase[SEC_COUNT];
    gbase[SEC_TEXT] = (long)TEXT_BASE;
    gbase[SEC_DATA] = align_up(gbase[SEC_TEXT] + total[SEC_TEXT], 4);
    gbase[SEC_BSS]  = align_up(gbase[SEC_DATA] + total[SEC_DATA], 4);

    /* base de cada (modulo, seccion) = base global + acumulado de previos */
    long (*mbase)[SEC_COUNT] = xcalloc((size_t)M, sizeof *mbase);
    long acc[SEC_COUNT] = {0, 0, 0};
    for (int j = 0; j < M; j++) {
        for (int s = 0; s < SEC_COUNT; s++) {
            mbase[j][s] = gbase[s] + acc[s];
            acc[s] += objs[j].sec_size[s];
        }
    }

    /* --- buffers fusionados de .text y .data --- */
    ByteBuf mtext, mdata;
    bb_init(&mtext); bb_init(&mdata);
    for (int j = 0; j < M; j++) {
        bb_append(&mtext, objs[j].sec_bytes[SEC_TEXT].data, objs[j].sec_bytes[SEC_TEXT].len);
        bb_append(&mdata, objs[j].sec_bytes[SEC_DATA].data, objs[j].sec_bytes[SEC_DATA].len);
    }

    /* --- mapa de simbolos GLOBAL (con deteccion de duplicados) --- */
    GlobalSym *globs = NULL; size_t nglob = 0, gcap = 0;
    int errors = 0;
    for (int j = 0; j < M; j++) {
        for (size_t i = 0; i < objs[j].nsyms; i++) {
            Symbol *s = &objs[j].syms[i];
            if (s->kind == SYM_LABEL && s->is_global && s->defined) {
                for (size_t k = 0; k < nglob; k++)
                    if (strcmp(globs[k].name, s->name) == 0) {
                        fprintf(stderr, "error: simbolo global duplicado '%s'\n", s->name);
                        errors++;
                    }
                if (nglob == gcap) {
                    gcap = gcap ? gcap * 2 : 16;
                    globs = xrealloc(globs, gcap * sizeof(GlobalSym));
                }
                GlobalSym *g = &globs[nglob++];
                strncpy(g->name, s->name, MAXNAME - 1);
                g->name[MAXNAME - 1] = '\0';
                g->module   = j;
                g->section  = s->section;
                g->value    = s->value;
                g->is_const = 0;
            }
        }
    }

    /* --- aplica relocaciones --- */
    long relocs_applied = 0;
    for (int j = 0; j < M; j++) {
        for (size_t i = 0; i < objs[j].nreloc; i++) {
            Reloc *r = &objs[j].relocs[i];

            long sym_final;
            int ok = resolve_symbol(objs, mbase, globs, nglob, j, r->sym, &sym_final);
            if (!ok) {
                fprintf(stderr, "error: simbolo externo no resuelto '%s' (modulo %s)\n",
                        r->sym, objs[j].name);
                errors++;
                continue;
            }

            long site_final = mbase[j][r->section] + r->offset;
            unsigned int value;
            if (r->kind == REL_ABS32)
                value = (unsigned int)(sym_final + r->addend);
            else /* REL_REL32 */
                value = (unsigned int)(sym_final + r->addend - (site_final + 4));

            ByteBuf *buf = (r->section == SEC_TEXT) ? &mtext :
                           (r->section == SEC_DATA) ? &mdata : NULL;
            if (!buf) {
                fprintf(stderr, "aviso: relocacion en .bss ignorada\n");
                continue;
            }
            long idx = site_final - gbase[r->section];   /* indice en el buffer */
            if (idx < 0 || idx + 4 > (long)buf->len) {
                fprintf(stderr, "error: relocacion fuera de rango (sym %s)\n", r->sym);
                errors++;
                continue;
            }
            bb_patch32(buf, (size_t)idx, value);
            relocs_applied++;
        }
    }

    /* --- escribe el binario final en hexadecimal --- */
    FILE *f = fopen(outpath, "w");
    if (!f) die("no se pudo crear '%s'", outpath);

    fprintf(f, "; Imagen final enlazada (hexadecimal) - ia32 mini linker\n");
    fprintf(f, "; modulos: %d\n", M);
    fprintf(f, "; .text base=0x%08lx size=%zu\n", gbase[SEC_TEXT], mtext.len);
    fprintf(f, "; .data base=0x%08lx size=%zu\n", gbase[SEC_DATA], mdata.len);
    fprintf(f, "; .bss  base=0x%08lx size=%ld (reservada, sin contenido)\n",
            gbase[SEC_BSS], total[SEC_BSS]);

    /* Vuelca un buffer con prefijo de direccion (16 bytes por linea). */
    #define DUMP_SECTION(title, buf, base) do {                               \
        fprintf(f, "\n%s\n", (title));                                        \
        for (size_t _i = 0; _i < (buf).len; _i += 16) {                       \
            fprintf(f, "%08lx:", (base) + (long)_i);                          \
            for (size_t _k = _i; _k < _i + 16 && _k < (buf).len; _k++)        \
                fprintf(f, " %02X", (buf).data[_k]);                          \
            fprintf(f, "\n");                                                 \
        }                                                                     \
        if ((buf).len == 0) fprintf(f, "; (vacia)\n");                        \
    } while (0)

    DUMP_SECTION(".text", mtext, gbase[SEC_TEXT]);
    DUMP_SECTION(".data", mdata, gbase[SEC_DATA]);
    fprintf(f, "\n.bss\n");
    fprintf(f, "%08lx: (se reservan %ld bytes en cero)\n",
            gbase[SEC_BSS], total[SEC_BSS]);
    fclose(f);

    /* --- escribe el mapa de simbolos --- */
    char mappath[512];
    {
        size_t n = strlen(outpath);
        if (n > 4 && strcmp(outpath + n - 4, ".hex") == 0)
            snprintf(mappath, sizeof mappath, "%.*s_map.txt", (int)(n - 4), outpath);
        else
            snprintf(mappath, sizeof mappath, "%s_map.txt", outpath);
    }
    FILE *mf = fopen(mappath, "w");
    if (mf) {
        fprintf(mf, "# Mapa de simbolos (direcciones finales)\n");
        fprintf(mf, "# %-20s %-8s %-10s %s\n", "SIMBOLO", "MODULO", "SECCION", "DIRECCION");
        for (int j = 0; j < M; j++) {
            for (size_t i = 0; i < objs[j].nsyms; i++) {
                Symbol *s = &objs[j].syms[i];
                if (s->kind == SYM_LABEL && s->defined) {
                    long addr = mbase[j][s->section] + s->value;
                    fprintf(mf, "%-22s %-8d %-10s 0x%08lx%s\n",
                            s->name, j, section_name(s->section), addr,
                            s->is_global ? "  (global)" : "");
                } else if (s->kind == SYM_CONST) {
                    fprintf(mf, "%-22s %-8d %-10s 0x%08lx  (const)\n",
                            s->name, j, "-", s->value);
                }
            }
        }
        fclose(mf);
    }

    /* --- resumen por stdout --- */
    printf("== minilinker ==\n");
    printf("  modulos        : %d\n", M);
    for (int j = 0; j < M; j++)
        printf("    [%d] %-20s text=%ld data=%ld bss=%ld\n",
               j, objs[j].name,
               objs[j].sec_size[SEC_TEXT], objs[j].sec_size[SEC_DATA],
               objs[j].sec_size[SEC_BSS]);
    printf("  .text final    : 0x%08lx (%zu bytes)\n", gbase[SEC_TEXT], mtext.len);
    printf("  .data final    : 0x%08lx (%zu bytes)\n", gbase[SEC_DATA], mdata.len);
    printf("  .bss  final    : 0x%08lx (%ld bytes)\n", gbase[SEC_BSS], total[SEC_BSS]);
    printf("  globales       : %zu\n", nglob);
    printf("  reubicaciones  : %ld aplicadas\n", relocs_applied);
    printf("  salida         : %s\n", outpath);
    printf("  mapa           : %s\n", mappath);
    if (errors) printf("  ATENCION       : %d error(es) de enlace.\n", errors);

    /* --- limpieza --- */
    bb_free(&mtext); bb_free(&mdata);
    free(globs);
    free(mbase);
    for (int j = 0; j < M; j++) objfile_free(&objs[j]);
    free(objs);

    return errors ? 1 : 0;
}
