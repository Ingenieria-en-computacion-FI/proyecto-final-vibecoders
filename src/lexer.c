/* ============================================================================
 * lexer.c  -  Analizador lexico manual
 *
 * Lee el texto fuente caracter por caracter y devuelve un token a la vez.
 * No usa flex ni ninguna libreria de analisis lexico.
 * ==========================================================================*/
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "lexer.h"
#include "util.h"

void lexer_init(Lexer *lx, const char *src, const char *filename) {
    lx->src      = src;
    lx->pos      = 0;
    lx->len      = strlen(src);
    lx->line     = 1;
    lx->filename = filename;
}

/* Caracter actual sin avanzar (o '\0' al final). */
static int peek(Lexer *lx) {
    return (lx->pos < lx->len) ? (unsigned char)lx->src[lx->pos] : '\0';
}

/* Avanza un caracter y lo devuelve. */
static int advance(Lexer *lx) {
    int c = peek(lx);
    if (c != '\0') lx->pos++;
    return c;
}

/* Un identificador inicia con letra, '_', '.' (para .text) y continua con
   letras, digitos, '_' y '.'. */
static int is_ident_start(int c) {
    return isalpha(c) || c == '_' || c == '.';
}
static int is_ident_char(int c) {
    return isalnum(c) || c == '_' || c == '.';
}

/* Salta espacios (no saltos de linea) y comentarios ';' hasta fin de linea. */
static void skip_blanks_and_comments(Lexer *lx) {
    for (;;) {
        int c = peek(lx);
        if (c == ' ' || c == '\t' || c == '\r') {
            advance(lx);
        } else if (c == ';') {                 /* comentario hasta fin linea */
            while (peek(lx) != '\n' && peek(lx) != '\0') advance(lx);
        } else {
            break;
        }
    }
}

/* Construye un token simple de un caracter. */
static Token simple(Lexer *lx, TokenType t) {
    Token tok;
    memset(&tok, 0, sizeof tok);
    tok.type = t;
    tok.line = lx->line;
    tok.reg  = REG_NONE;
    return tok;
}

Token lexer_next(Lexer *lx) {
    skip_blanks_and_comments(lx);

    Token tok;
    memset(&tok, 0, sizeof tok);
    tok.line = lx->line;
    tok.reg  = REG_NONE;

    int c = peek(lx);

    if (c == '\0') { tok.type = T_EOF; return tok; }

    if (c == '\n') {
        advance(lx);
        tok.type = T_NEWLINE;
        lx->line++;
        return tok;
    }

    /* Puntuacion de un caracter. */
    switch (c) {
        case ',': advance(lx); return simple(lx, T_COMMA);
        case '[': advance(lx); return simple(lx, T_LBRACKET);
        case ']': advance(lx); return simple(lx, T_RBRACKET);
        case '+': advance(lx); return simple(lx, T_PLUS);
        case '-': advance(lx); return simple(lx, T_MINUS);
        case '*': advance(lx); return simple(lx, T_STAR);
        case ':': advance(lx); return simple(lx, T_COLON);
        default: break;
    }

    /* Cadena entre comillas dobles (para db). */
    if (c == '"') {
        advance(lx);                            /* consume la comilla inicial */
        int n = 0;
        while (peek(lx) != '"' && peek(lx) != '\0' && peek(lx) != '\n') {
            int ch = advance(lx);
            if (n < (int)sizeof(tok.text) - 1) tok.text[n++] = (char)ch;
        }
        if (peek(lx) == '"') advance(lx);        /* consume la comilla final  */
        else error_at(lx->filename, lx->line, "cadena sin comilla de cierre");
        tok.text[n] = '\0';
        tok.type = T_STRING;
        return tok;
    }

    /* Numero: decimal o hexadecimal 0x... */
    if (isdigit(c)) {
        char buf[MAXNAME]; int n = 0;
        int base = 10;
        if (c == '0') {
            buf[n++] = (char)advance(lx);
            if (peek(lx) == 'x' || peek(lx) == 'X') {
                buf[n++] = (char)advance(lx);
                base = 16;
            }
        }
        while (isalnum(peek(lx))) {
            if (n < (int)sizeof(buf) - 1) buf[n++] = (char)advance(lx);
            else advance(lx);
        }
        buf[n] = '\0';
        tok.type  = T_NUMBER;
        tok.value = (long)strtol(buf, NULL, base);
        return tok;
    }

    /* Identificador, registro o mnemonico. */
    if (is_ident_start(c)) {
        int n = 0;
        char buf[MAXNAME];
        while (is_ident_char(peek(lx))) {
            if (n < (int)sizeof(buf) - 1) buf[n++] = (char)advance(lx);
            else advance(lx);
        }
        buf[n] = '\0';

        Register r = reg_from_name(buf);
        if (r != REG_NONE) {
            tok.type = T_REGISTER;
            tok.reg  = r;
            strncpy(tok.text, buf, MAXNAME - 1);
            return tok;
        }
        tok.type = T_IDENT;
        strncpy(tok.text, buf, MAXNAME - 1);
        return tok;
    }

    /* Caracter no reconocido: lo reportamos y lo saltamos. */
    error_at(lx->filename, lx->line, "caracter inesperado '%c' (0x%02x)", c, c);
    advance(lx);
    return lexer_next(lx);
}
