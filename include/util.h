/* ============================================================================
 * util.h  -  Utilidades generales
 *  - Reporte de errores
 *  - Memoria segura (xmalloc / xrealloc / xstrdup)
 *  - Buffer de bytes dinamico (ByteBuf) usado para el codigo de cada seccion
 *  - Lectura completa de un archivo a memoria
 * ==========================================================================*/
#ifndef IA32_UTIL_H
#define IA32_UTIL_H

#include <stddef.h>

/* Errores -------------------------------------------------------------------*/
void  die(const char *fmt, ...);                       /* error fatal -> exit */
void  warn(const char *fmt, ...);                      /* advertencia         */
void  error_at(const char *file, int line,
               const char *fmt, ...);                  /* error con ubicacion */

/* Memoria -------------------------------------------------------------------*/
void *xmalloc(size_t n);
void *xcalloc(size_t n, size_t sz);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);

/* Lectura de archivo --------------------------------------------------------*/
/* Devuelve el contenido completo del archivo en un buffer terminado en '\0'.
   El llamador debe liberar con free(). Aborta si no se puede abrir. */
char *read_file(const char *path);

/* Buffer de bytes dinamico --------------------------------------------------*/
typedef struct {
    unsigned char *data;
    size_t         len;
    size_t         cap;
} ByteBuf;

void bb_init  (ByteBuf *b);
void bb_free  (ByteBuf *b);
void bb_push  (ByteBuf *b, unsigned char byte);
void bb_append(ByteBuf *b, const unsigned char *src, size_t n);
void bb_put32 (ByteBuf *b, unsigned int v);            /* little-endian       */
void bb_patch32(ByteBuf *b, size_t off, unsigned int v);
void bb_grow_zero(ByteBuf *b, size_t n);               /* añade n ceros       */

#endif /* IA32_UTIL_H */
