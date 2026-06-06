/* ============================================================================
 * lexer.h  -  Analizador lexico MANUAL (sin lex/flex)
 *
 * Responsabilidad (Integrante 1):
 *   - Leer el codigo fuente caracter por caracter y producir tokens.
 *   - Reconocer registros, numeros, identificadores, simbolos de puntuacion.
 *   - Ignorar comentarios (';' hasta fin de linea) y espacios.
 *   - Tratar el salto de linea como token significativo (las sentencias del
 *     ensamblador son por linea).
 * ==========================================================================*/
#ifndef IA32_LEXER_H
#define IA32_LEXER_H

#include <stddef.h>
#include "types.h"

typedef enum {
    T_EOF,         /* fin de archivo                                   */
    T_NEWLINE,     /* fin de linea (separador de sentencias)           */
    T_IDENT,       /* mnemonico, etiqueta o directiva                  */
    T_REGISTER,    /* eax, ebx, ... (reconocido por el lexer)          */
    T_NUMBER,      /* literal numerico (decimal o 0xHEX)               */
    T_STRING,      /* cadena entre comillas dobles (para db)           */
    T_COMMA,       /* ,                                                */
    T_LBRACKET,    /* [                                                */
    T_RBRACKET,    /* ]                                                */
    T_PLUS,        /* +                                                */
    T_MINUS,       /* -                                                */
    T_STAR,        /* *  (escala SIB)                                  */
    T_COLON        /* :  (definicion de etiqueta)                      */
} TokenType;

typedef struct {
    TokenType type;
    char      text[MAXNAME];  /* lexema, para IDENT / STRING / REGISTER       */
    long      value;          /* valor entero si type == T_NUMBER             */
    Register  reg;            /* registro si type == T_REGISTER               */
    int       line;           /* linea donde aparece (para errores)           */
} Token;

typedef struct {
    const char *src;          /* texto fuente completo (en memoria)           */
    size_t      pos;          /* posicion actual                              */
    size_t      len;          /* longitud total                               */
    int         line;         /* linea actual (1-based)                       */
    const char *filename;     /* nombre del archivo (para mensajes)           */
} Lexer;

void  lexer_init(Lexer *lx, const char *src, const char *filename);
Token lexer_next(Lexer *lx);   /* devuelve el siguiente token                 */

#endif /* IA32_LEXER_H */
