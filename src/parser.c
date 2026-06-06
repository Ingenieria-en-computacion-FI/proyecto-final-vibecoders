/* ============================================================================
 * parser.c  -  Analizador sintactico manual
 *
 * Convierte la secuencia de tokens en una lista de "Statement" (una por linea
 * logica). Reconoce etiquetas, directivas, instrucciones y los modos de
 * direccionamiento soportados. No usa yacc/bison.
 * ==========================================================================*/
#include <string.h>
#include <ctype.h>
#include "parser.h"
#include "util.h"

/* Comparacion sin distinguir mayusculas. */
static int ieq(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == *b;
}

static Token next_tok(Parser *p) {
    Token t = p->cur;
    p->cur = lexer_next(&p->lx);
    return t;
}

void parser_init(Parser *p, const char *src, const char *filename) {
    lexer_init(&p->lx, src, filename);
    p->filename    = filename;
    p->error_count = 0;
    p->cur         = lexer_next(&p->lx);   /* carga el primer token (lookahead) */
}

static int is_directive_kw(const char *s) {
    return ieq(s, "section") || ieq(s, "global") || ieq(s, "extern") ||
           ieq(s, "db")  || ieq(s, "dw")  || ieq(s, "dd") ||
           ieq(s, "resb")|| ieq(s, "resw")|| ieq(s, "resd") ||
           ieq(s, "org");
}

/* -------------------------------------------------------------------------
 * Parseo de una referencia a memoria  [ base + index*scale + disp ]
 * Recibe los tokens internos (sin los corchetes).
 * ----------------------------------------------------------------------- */
static int parse_mem(Parser *p, Token *t, int n, Operand *op, int line) {
    op->kind  = OPND_MEM;
    op->base  = REG_NONE;
    op->index = REG_NONE;
    op->scale = 1;
    op->disp  = 0;
    op->has_sym = 0;
    op->sym[0]  = '\0';

    int i = 0;
    int sign = 1;

    if (n == 0) {
        error_at(p->filename, line, "operando de memoria vacio []");
        p->error_count++;
        return 0;
    }

    while (i < n) {
        if (t[i].type == T_PLUS)  { sign =  1; i++; continue; }
        if (t[i].type == T_MINUS) { sign = -1; i++; continue; }

        if (t[i].type == T_REGISTER) {
            Register r = t[i].reg;
            i++;
            if (i < n && t[i].type == T_STAR) {
                /* registro escalado -> indice */
                i++;
                if (i >= n || t[i].type != T_NUMBER) {
                    error_at(p->filename, line, "se esperaba escala despues de '*'");
                    p->error_count++; return 0;
                }
                long sc = t[i].value; i++;
                if (sc != 1 && sc != 2 && sc != 4 && sc != 8) {
                    error_at(p->filename, line, "escala SIB invalida: %ld (use 1,2,4,8)", sc);
                    p->error_count++; return 0;
                }
                if (op->index != REG_NONE) {
                    error_at(p->filename, line, "solo se permite un registro indice");
                    p->error_count++; return 0;
                }
                op->index = r;
                op->scale = (int)sc;
            } else {
                /* registro simple: base, o indice si ya hay base */
                if (op->base == REG_NONE) {
                    op->base = r;
                } else if (op->index == REG_NONE) {
                    op->index = r;
                    op->scale = 1;
                } else {
                    error_at(p->filename, line, "demasiados registros en [ ]");
                    p->error_count++; return 0;
                }
            }
            sign = 1;
        } else if (t[i].type == T_NUMBER) {
            op->disp += sign * t[i].value;
            i++; sign = 1;
        } else if (t[i].type == T_IDENT) {
            if (op->has_sym) {
                error_at(p->filename, line, "solo se permite un simbolo en [ ]");
                p->error_count++; return 0;
            }
            op->has_sym = 1;
            strncpy(op->sym, t[i].text, MAXNAME - 1);
            i++; sign = 1;
        } else {
            error_at(p->filename, line, "token invalido dentro de [ ]");
            p->error_count++; return 0;
        }
    }

    if (op->index == REG_ESP) {
        error_at(p->filename, line, "ESP no puede usarse como indice");
        p->error_count++; return 0;
    }
    return 1;
}

/* -------------------------------------------------------------------------
 * Parseo de un operando individual (registro / inmediato / memoria / simbolo).
 * ----------------------------------------------------------------------- */
static int parse_operand(Parser *p, Token *t, int n, Operand *op, int line) {
    memset(op, 0, sizeof *op);
    op->base = op->index = REG_NONE;
    op->scale = 1;

    if (n == 0) {
        error_at(p->filename, line, "operando vacio");
        p->error_count++; return 0;
    }

    /* Memoria: [ ... ] */
    if (t[0].type == T_LBRACKET) {
        if (t[n - 1].type != T_RBRACKET) {
            error_at(p->filename, line, "falta ']' de cierre");
            p->error_count++; return 0;
        }
        return parse_mem(p, t + 1, n - 2, op, line);
    }

    /* Registro */
    if (t[0].type == T_REGISTER && n == 1) {
        op->kind = OPND_REG;
        op->reg  = t[0].reg;
        return 1;
    }

    /* Inmediato negativo: - numero */
    if (t[0].type == T_MINUS && n == 2 && t[1].type == T_NUMBER) {
        op->kind = OPND_IMM;
        op->imm  = -t[1].value;
        return 1;
    }

    /* Inmediato literal */
    if (t[0].type == T_NUMBER && n == 1) {
        op->kind = OPND_IMM;
        op->imm  = t[0].value;
        return 1;
    }

    /* Simbolo usado como inmediato/direccion (p.ej. jmp etiqueta, mov eax,msg) */
    if (t[0].type == T_IDENT) {
        op->kind    = OPND_IMM;
        op->has_sym = 1;
        op->imm     = 0;
        strncpy(op->sym, t[0].text, MAXNAME - 1);
        /* permite  simbolo + numero  /  simbolo - numero */
        if (n >= 3 && (t[1].type == T_PLUS || t[1].type == T_MINUS) &&
            t[2].type == T_NUMBER) {
            op->imm = (t[1].type == T_PLUS) ? t[2].value : -t[2].value;
        } else if (n != 1) {
            error_at(p->filename, line, "operando simbolico mal formado");
            p->error_count++; return 0;
        }
        return 1;
    }

    error_at(p->filename, line, "operando no reconocido");
    p->error_count++;
    return 0;
}

/* Agrega un valor de byte/word/dword (o caracter de cadena) a la directiva. */
static void add_dval(Statement *out, long v) {
    if (out->ndargs < 64) {
        out->dvals[out->ndargs]     = v;
        out->dsym_flag[out->ndargs] = 0;
        out->dsyms[out->ndargs][0]  = '\0';
        out->ndargs++;
    }
}
static void add_dsym(Statement *out, const char *s) {
    if (out->ndargs < 64) {
        out->dvals[out->ndargs]     = 0;
        out->dsym_flag[out->ndargs] = 1;
        strncpy(out->dsyms[out->ndargs], s, MAXNAME - 1);
        out->ndargs++;
    }
}

/* Parseo de los argumentos de db/dw/dd (lista separada por comas). */
static void parse_data_args(Parser *p, Token *t, int start, int n, Statement *out) {
    int i = start;
    while (i < n) {
        if (t[i].type == T_NUMBER) {
            add_dval(out, t[i].value); i++;
        } else if (t[i].type == T_MINUS && i + 1 < n && t[i+1].type == T_NUMBER) {
            add_dval(out, -t[i+1].value); i += 2;
        } else if (t[i].type == T_IDENT) {
            add_dsym(out, t[i].text); i++;     /* simbolo (direccion) en dd    */
        } else if (t[i].type == T_STRING) {
            /* una cadena se expande a un byte por caracter */
            for (const char *s = t[i].text; *s; s++) add_dval(out, (unsigned char)*s);
            i++;
        } else {
            error_at(p->filename, out->line, "argumento de datos invalido");
            p->error_count++; i++;
        }
        if (i < n) {
            if (t[i].type == T_COMMA) i++;
            else {
                error_at(p->filename, out->line, "se esperaba ',' entre datos");
                p->error_count++;
                break;
            }
        }
    }
}

int parser_next(Parser *p, Statement *out) {
    /* Salta lineas en blanco. */
    while (p->cur.type == T_NEWLINE) next_tok(p);
    if (p->cur.type == T_EOF) return 0;

    /* Recolecta todos los tokens de la linea logica. */
    Token toks[128];
    int nt = 0;
    int line = p->cur.line;
    while (p->cur.type != T_NEWLINE && p->cur.type != T_EOF) {
        if (nt < 128) toks[nt++] = next_tok(p);
        else          next_tok(p);
    }
    if (p->cur.type == T_NEWLINE) next_tok(p);   /* consume el salto de linea  */

    memset(out, 0, sizeof *out);
    out->type = ST_NONE;
    out->line = line;
    for (int k = 0; k < MAX_OPERANDS; k++) {
        out->ops[k].base = out->ops[k].index = REG_NONE;
        out->ops[k].scale = 1;
    }

    int i = 0;

    /* Etiqueta opcional:  IDENT ':' */
    if (nt >= 2 && toks[0].type == T_IDENT && toks[1].type == T_COLON) {
        out->has_label = 1;
        strncpy(out->label, toks[0].text, MAXNAME - 1);
        i = 2;
    }

    if (i >= nt) {                       /* solo habia una etiqueta            */
        out->type = out->has_label ? ST_LABEL_ONLY : ST_NONE;
        return out->type != ST_NONE ? 1 : parser_next(p, out);
    }

    Token head = toks[i];

    /* Definicion EQU:  NOMBRE equ VALOR */
    if (head.type == T_IDENT && i + 1 < nt &&
        toks[i+1].type == T_IDENT && ieq(toks[i+1].text, "equ")) {
        out->type = ST_DIRECTIVE;
        strncpy(out->directive, "equ", sizeof out->directive - 1);
        strncpy(out->dname, head.text, MAXNAME - 1);
        if (i + 2 < nt) {
            if (toks[i+2].type == T_NUMBER) {
                add_dval(out, toks[i+2].value);
            } else if (toks[i+2].type == T_MINUS && i + 3 < nt &&
                       toks[i+3].type == T_NUMBER) {
                add_dval(out, -toks[i+3].value);
            } else if (toks[i+2].type == T_IDENT) {
                add_dsym(out, toks[i+2].text);
            } else {
                error_at(p->filename, line, "valor de EQU invalido");
                p->error_count++;
            }
        } else {
            error_at(p->filename, line, "EQU sin valor");
            p->error_count++;
        }
        return 1;
    }

    /* Directiva con palabra clave al inicio. */
    if (head.type == T_IDENT && is_directive_kw(head.text)) {
        out->type = ST_DIRECTIVE;
        strncpy(out->directive, head.text, sizeof out->directive - 1);

        if (ieq(head.text, "section") || ieq(head.text, "global") ||
            ieq(head.text, "extern")) {
            if (i + 1 < nt && (toks[i+1].type == T_IDENT)) {
                strncpy(out->dname, toks[i+1].text, MAXNAME - 1);
            } else {
                error_at(p->filename, line, "'%s' requiere un nombre", head.text);
                p->error_count++;
            }
        } else if (ieq(head.text, "db") || ieq(head.text, "dw") ||
                   ieq(head.text, "dd")) {
            parse_data_args(p, toks, i + 1, nt, out);
        } else if (ieq(head.text, "resb") || ieq(head.text, "resw") ||
                   ieq(head.text, "resd")) {
            if (i + 1 < nt && toks[i+1].type == T_NUMBER) {
                add_dval(out, toks[i+1].value);
            } else {
                error_at(p->filename, line, "'%s' requiere una cantidad", head.text);
                p->error_count++;
            }
        } else if (ieq(head.text, "org")) {
            if (i + 1 < nt && toks[i+1].type == T_NUMBER) {
                add_dval(out, toks[i+1].value);
            } else {
                error_at(p->filename, line, "ORG requiere un valor");
                p->error_count++;
            }
        }
        return 1;
    }

    /* En otro caso: instruccion. */
    if (head.type != T_IDENT) {
        error_at(p->filename, line, "se esperaba un mnemonico");
        p->error_count++;
        return parser_next(p, out);
    }

    out->type = ST_INSTR;
    strncpy(out->mnemonic, head.text, MAXNAME - 1);
    i++;   /* avanza tras el mnemonico */

    /* Operandos separados por comas (a nivel superior, sin anidamiento). */
    int start = i;
    while (start < nt) {
        int end = start;
        while (end < nt && toks[end].type != T_COMMA) end++;
        if (out->nops >= MAX_OPERANDS) {
            error_at(p->filename, line, "demasiados operandos (max %d)", MAX_OPERANDS);
            p->error_count++;
            break;
        }
        if (!parse_operand(p, toks + start, end - start, &out->ops[out->nops], line)) {
            /* error ya reportado */
        }
        out->nops++;
        start = (end < nt) ? end + 1 : end;  /* salta la coma */
    }

    return 1;
}
