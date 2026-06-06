/* ============================================================================
 * util.c  -  Implementacion de utilidades generales
 * ==========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "util.h"

/* --- Errores ------------------------------------------------------------- */
void die(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    fprintf(stderr, "error: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

void warn(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    fprintf(stderr, "aviso: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

void error_at(const char *file, int line, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    fprintf(stderr, "%s:%d: error: ", file ? file : "?", line);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

/* --- Memoria ------------------------------------------------------------- */
void *xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if (!p) die("sin memoria (malloc %zu)", n);
    return p;
}

void *xcalloc(size_t n, size_t sz) {
    void *p = calloc(n ? n : 1, sz ? sz : 1);
    if (!p) die("sin memoria (calloc %zu x %zu)", n, sz);
    return p;
}

void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) die("sin memoria (realloc %zu)", n);
    return q;
}

char *xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = xmalloc(n);
    memcpy(p, s, n);
    return p;
}

/* --- Lectura de archivo -------------------------------------------------- */
char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) die("no se pudo abrir el archivo '%s'", path);

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) die("no se pudo medir '%s'", path);
    fseek(f, 0, SEEK_SET);

    char *buf = xmalloc((size_t)sz + 1);
    size_t rd = fread(buf, 1, (size_t)sz, f);
    buf[rd] = '\0';
    fclose(f);
    return buf;
}

/* --- Buffer de bytes ----------------------------------------------------- */
void bb_init(ByteBuf *b) {
    b->data = NULL; b->len = 0; b->cap = 0;
}

void bb_free(ByteBuf *b) {
    free(b->data);
    b->data = NULL; b->len = 0; b->cap = 0;
}

static void bb_reserve(ByteBuf *b, size_t extra) {
    if (b->len + extra <= b->cap) return;
    size_t ncap = b->cap ? b->cap * 2 : 64;
    while (ncap < b->len + extra) ncap *= 2;
    b->data = xrealloc(b->data, ncap);
    b->cap = ncap;
}

void bb_push(ByteBuf *b, unsigned char byte) {
    bb_reserve(b, 1);
    b->data[b->len++] = byte;
}

void bb_append(ByteBuf *b, const unsigned char *src, size_t n) {
    bb_reserve(b, n);
    memcpy(b->data + b->len, src, n);
    b->len += n;
}

void bb_put32(ByteBuf *b, unsigned int v) {
    bb_push(b, (unsigned char)( v        & 0xFF));
    bb_push(b, (unsigned char)((v >> 8)  & 0xFF));
    bb_push(b, (unsigned char)((v >> 16) & 0xFF));
    bb_push(b, (unsigned char)((v >> 24) & 0xFF));
}

void bb_patch32(ByteBuf *b, size_t off, unsigned int v) {
    if (off + 4 > b->len) die("bb_patch32 fuera de rango (off=%zu len=%zu)",
                              off, b->len);
    b->data[off + 0] = (unsigned char)( v        & 0xFF);
    b->data[off + 1] = (unsigned char)((v >> 8)  & 0xFF);
    b->data[off + 2] = (unsigned char)((v >> 16) & 0xFF);
    b->data[off + 3] = (unsigned char)((v >> 24) & 0xFF);
}

void bb_grow_zero(ByteBuf *b, size_t n) {
    bb_reserve(b, n);
    memset(b->data + b->len, 0, n);
    b->len += n;
}
