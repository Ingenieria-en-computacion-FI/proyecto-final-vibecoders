/* ============================================================================
 * objfile.h  -  Formato objeto y tablas de salida
 *
 * Responsabilidad (Integrante 5):
 *   - Definir el formato del archivo objeto (.o) en texto.
 *   - Escribir el .o (encabezado, secciones, simbolos, relocaciones, codigo).
 *   - Leer el .o (lo usa el mini linker).
 *   - Volcar las tablas .txt que pide el proyecto.
 *
 * El formato es de texto (legible) para facilitar la inspeccion y depuracion
 * en un proyecto escolar. La gramatica completa esta en docs/formato_objeto.md.
 * ==========================================================================*/
#ifndef IA32_OBJFILE_H
#define IA32_OBJFILE_H

#include <stddef.h>
#include "types.h"
#include "util.h"
#include "symtab.h"

/* Una relocacion final, tal como se guarda en el .o. */
typedef struct {
    SectionId section;       /* seccion donde esta el campo a parchar         */
    long      offset;        /* offset del campo dentro de la seccion         */
    char      sym[MAXNAME];  /* simbolo objetivo                              */
    RelocKind kind;          /* ABS32 / REL32                                 */
    long      addend;        /* constante adicional                           */
} Reloc;

/* -------------------------------------------------------------------------
 * Modulo objeto en construccion (lado ensamblador / escritura).
 * ----------------------------------------------------------------------- */
typedef struct {
    ByteBuf text;            /* bytes de .text                                */
    ByteBuf data;            /* bytes de .data                                */
    long    bss_size;        /* tamaño reservado en .bss                      */
    SectionId cur;           /* seccion actual durante el ensamblado          */

    long    org[SEC_COUNT];  /* origen opcional (ORG); solo informativo       */

    SymTab *symtab;          /* tabla de simbolos (referencia, no se libera)  */

    Reloc  *relocs;
    size_t  nreloc, reloc_cap;

    char    srcname[256];    /* archivo fuente de origen                      */
} ObjModule;

void obj_module_init(ObjModule *m, SymTab *st);
void obj_module_free(ObjModule *m);
void obj_add_reloc  (ObjModule *m, SectionId sec, long off,
                     const char *sym, RelocKind kind, long addend);
long obj_section_len(const ObjModule *m, SectionId s);

/* Escribe el archivo objeto completo. Devuelve 0 si todo bien. */
int  obj_write(const char *path, ObjModule *m);

/* -------------------------------------------------------------------------
 * Modulo objeto leido desde disco (lado linker / lectura).
 * ----------------------------------------------------------------------- */
typedef struct {
    char    name[256];
    long    sec_size[SEC_COUNT];
    ByteBuf sec_bytes[SEC_COUNT];   /* .bss queda vacio (solo tamaño)         */

    Symbol *syms;  size_t nsyms,  sym_cap;
    Reloc  *relocs; size_t nreloc, reloc_cap;
} ObjFile;

int  obj_read(const char *path, ObjFile *o);   /* 0 si exito                  */
void objfile_free(ObjFile *o);

/* -------------------------------------------------------------------------
 * Volcado de tablas .txt requeridas por el proyecto.
 * ----------------------------------------------------------------------- */
void dump_symbols   (const char *path, SymTab *t);
void dump_sections  (const char *path, const ObjModule *m);
void dump_relocations(const char *path, const ObjModule *m);

#endif /* IA32_OBJFILE_H */
