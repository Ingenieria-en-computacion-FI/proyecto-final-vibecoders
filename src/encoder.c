/* ============================================================================
 * encoder.c  -  Generacion de codigo maquina IA-32 (subconjunto del proyecto)
 *
 * Construye, segun la instruccion:  [opcode][ModRM][SIB][desplazamiento][imm]
 *
 * Las decisiones de codificacion (que opcode usa cada instruccion, como se
 * arma ModRM/SIB, etc.) estan documentadas en docs/formato_objeto.md.
 * ==========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "encoder.h"
#include "util.h"

/* ---- utilidades ---- */
static int ieq(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == *b;
}
static int fits8(long v) { return v >= -128 && v <= 127; }
static int regcode(Register r) { return (int)r; }   /* 0..7 == campo de 3 bits */

static void emit(EncResult *r, unsigned char b) {
    if (r->len < (int)sizeof(r->bytes)) r->bytes[r->len++] = b;
    else { r->ok = 0; snprintf(r->err, sizeof r->err, "instruccion demasiado larga"); }
}
static void emit_u32(EncResult *r, unsigned int v) {
    emit(r, (unsigned char)( v        & 0xFF));
    emit(r, (unsigned char)((v >> 8)  & 0xFF));
    emit(r, (unsigned char)((v >> 16) & 0xFF));
    emit(r, (unsigned char)((v >> 24) & 0xFF));
}
static void add_ref(EncResult *r, int field_off, RelocKind kind,
                    const char *sym, long addend) {
    if (r->nrefs >= 4) { r->ok = 0; return; }
    EncRef *e = &r->refs[r->nrefs++];
    e->field_off = field_off;
    e->kind      = kind;
    e->addend    = addend;
    strncpy(e->sym, sym, MAXNAME - 1);
    e->sym[MAXNAME - 1] = '\0';
}
static void fail(EncResult *r, const char *msg) {
    r->ok = 0;
    strncpy(r->err, msg, sizeof r->err - 1);
}

/* Convierte la escala (1,2,4,8) a los 2 bits del campo scale del SIB. */
static int scale_bits(int s) {
    switch (s) { case 1: return 0; case 2: return 1;
                 case 4: return 2; case 8: return 3; default: return 0; }
}

/* ---------------------------------------------------------------------------
 * Emite ModRM (+ SIB + desplazamiento) para un operando r/m, usando reg_field
 * como los 3 bits del campo 'reg'. Si el operando de memoria referencia un
 * simbolo, registra una referencia ABS32 sobre el campo de 32 bits.
 * ------------------------------------------------------------------------- */
static void emit_modrm(EncResult *r, int reg_field, const Operand *rm) {
    reg_field &= 7;

    if (rm->kind == OPND_REG) {
        emit(r, (unsigned char)(0xC0 | (reg_field << 3) | regcode(rm->reg)));
        return;
    }
    if (rm->kind != OPND_MEM) { fail(r, "operando r/m invalido"); return; }

    Register base  = rm->base;
    Register index = rm->index;
    long     disp  = rm->disp;
    int      has_sym = rm->has_sym;

    int need_sib = (index != REG_NONE) || (base == REG_ESP);

    /* --- Caso 1: memoria directa  [disp] / [simbolo]  (sin base ni indice) */
    if (base == REG_NONE && index == REG_NONE) {
        emit(r, (unsigned char)(0x00 | (reg_field << 3) | 0x05)); /* rm=101    */
        if (has_sym) add_ref(r, r->len, REL_ABS32, rm->sym, disp);
        emit_u32(r, (unsigned int)disp);
        return;
    }

    /* --- Caso 2: con SIB (hay indice, o la base es ESP, o no hay base) ----- */
    if (need_sib || base == REG_NONE) {
        int base_code  = (base  == REG_NONE) ? 5 /*sin base*/ : regcode(base);
        int index_code = (index == REG_NONE) ? 4 /*sin indice*/ : regcode(index);

        int mod;
        int disp_size;   /* 0, 1 o 4 */

        if (base == REG_NONE) {
            mod = 0; disp_size = 4;            /* [index*scale + disp32]       */
        } else if (has_sym) {
            mod = 2; disp_size = 4;
        } else if (disp == 0 && base != REG_EBP) {
            mod = 0; disp_size = 0;
        } else if (fits8(disp)) {
            mod = 1; disp_size = 1;
        } else {
            mod = 2; disp_size = 4;
        }

        emit(r, (unsigned char)((mod << 6) | (reg_field << 3) | 0x04)); /* rm=100 */
        emit(r, (unsigned char)((scale_bits(rm->scale) << 6) |
                                (index_code << 3) | base_code));

        if (disp_size == 1) {
            emit(r, (unsigned char)(disp & 0xFF));
        } else if (disp_size == 4) {
            if (has_sym) add_ref(r, r->len, REL_ABS32, rm->sym, disp);
            emit_u32(r, (unsigned int)disp);
        }
        return;
    }

    /* --- Caso 3: solo base (+ desplazamiento) ------------------------------ */
    {
        int rm_code = regcode(base);
        int mod, disp_size;

        if (has_sym) {
            mod = 2; disp_size = 4;
        } else if (disp == 0 && base != REG_EBP) {
            mod = 0; disp_size = 0;
        } else if (fits8(disp)) {
            mod = 1; disp_size = 1;
        } else {
            mod = 2; disp_size = 4;
        }

        emit(r, (unsigned char)((mod << 6) | (reg_field << 3) | rm_code));

        if (disp_size == 1) {
            emit(r, (unsigned char)(disp & 0xFF));
        } else if (disp_size == 4) {
            if (has_sym) add_ref(r, r->len, REL_ABS32, rm->sym, disp);
            emit_u32(r, (unsigned int)disp);
        }
    }
}

/* Emite un inmediato de 32 bits que puede ser una direccion simbolica (ABS32). */
static void emit_imm32(EncResult *r, const Operand *op) {
    if (op->has_sym) {
        add_ref(r, r->len, REL_ABS32, op->sym, op->imm);   /* imm = addend     */
        emit_u32(r, (unsigned int)op->imm);
    } else {
        emit_u32(r, (unsigned int)op->imm);
    }
}

/* Tabla de instrucciones ALU de dos operandos. */
typedef struct {
    const char *name;
    unsigned    op_rm_r;   /* forma  r/m, reg   (estilo ADD 0x01)             */
    unsigned    op_r_rm;   /* forma  reg, r/m   (estilo ADD 0x03)             */
    int         digit;     /* extension para la forma inmediata (0x81 /digit) */
} AluOp;

static const AluOp kAlu[] = {
    { "add", 0x01, 0x03, 0 },
    { "or",  0x09, 0x0B, 1 },
    { "and", 0x21, 0x23, 4 },
    { "sub", 0x29, 0x2B, 5 },
    { "xor", 0x31, 0x33, 6 },
    { "cmp", 0x39, 0x3B, 7 },
};

static const AluOp *find_alu(const char *m) {
    for (size_t i = 0; i < sizeof kAlu / sizeof kAlu[0]; i++)
        if (ieq(m, kAlu[i].name)) return &kAlu[i];
    return NULL;
}

/* Saltos condicionales -> segundo byte del opcode (con prefijo 0F). */
static int jcc_opcode(const char *m) {
    if (ieq(m, "je"))  return 0x84;
    if (ieq(m, "jne")) return 0x85;
    if (ieq(m, "jg"))  return 0x8F;
    if (ieq(m, "jge")) return 0x8D;
    if (ieq(m, "jl"))  return 0x8C;
    if (ieq(m, "jle")) return 0x8E;
    return -1;
}

/* Grupo unario sobre r/m. Devuelve 1 si reconocio el mnemonico. */
static int unary_group(const char *m, unsigned *opcode, int *digit) {
    if (ieq(m, "inc"))  { *opcode = 0xFF; *digit = 0; return 1; }
    if (ieq(m, "dec"))  { *opcode = 0xFF; *digit = 1; return 1; }
    if (ieq(m, "not"))  { *opcode = 0xF7; *digit = 2; return 1; }
    if (ieq(m, "neg"))  { *opcode = 0xF7; *digit = 3; return 1; }
    if (ieq(m, "mul"))  { *opcode = 0xF7; *digit = 4; return 1; }
    if (ieq(m, "imul")) { *opcode = 0xF7; *digit = 5; return 1; }  /* 1 operando */
    if (ieq(m, "div"))  { *opcode = 0xF7; *digit = 6; return 1; }
    if (ieq(m, "idiv")) { *opcode = 0xF7; *digit = 7; return 1; }
    return 0;
}

/* ===========================================================================
 * encode_instruction
 * =========================================================================*/
EncResult encode_instruction(const Statement *st) {
    EncResult r;
    memset(&r, 0, sizeof r);
    r.ok = 1;

    const char *m = st->mnemonic;
    const Operand *a = (st->nops > 0) ? &st->ops[0] : NULL;
    const Operand *b = (st->nops > 1) ? &st->ops[1] : NULL;

    /* -------- instrucciones sin operandos -------- */
    if (ieq(m, "ret")) {
        if (st->nops) fail(&r, "ret no lleva operandos");
        else emit(&r, 0xC3);
        return r;
    }
    if (ieq(m, "nop")) {
        if (st->nops) fail(&r, "nop no lleva operandos");
        else emit(&r, 0x90);
        return r;
    }

    /* -------- int n -------- */
    if (ieq(m, "int")) {
        if (st->nops != 1 || a->kind != OPND_IMM || a->has_sym)
            fail(&r, "int requiere un inmediato (0..255)");
        else { emit(&r, 0xCD); emit(&r, (unsigned char)(a->imm & 0xFF)); }
        return r;
    }

    /* -------- saltos / llamadas (rel32) -------- */
    if (ieq(m, "jmp") || ieq(m, "call")) {
        if (st->nops != 1) { fail(&r, "se esperaba una etiqueta destino"); return r; }
        if (a->kind != OPND_IMM) { fail(&r, "solo se soporta salto/llamada a etiqueta"); return r; }
        emit(&r, ieq(m, "jmp") ? 0xE9 : 0xE8);
        if (a->has_sym) add_ref(&r, r.len, REL_REL32, a->sym, a->imm);
        emit_u32(&r, (unsigned int)a->imm);   /* placeholder / valor literal   */
        return r;
    }
    {
        int cc = jcc_opcode(m);
        if (cc >= 0) {
            if (st->nops != 1 || a->kind != OPND_IMM) { fail(&r, "salto condicional requiere etiqueta"); return r; }
            emit(&r, 0x0F);
            emit(&r, (unsigned char)cc);
            if (a->has_sym) add_ref(&r, r.len, REL_REL32, a->sym, a->imm);
            emit_u32(&r, (unsigned int)a->imm);
            return r;
        }
    }

    /* -------- push / pop -------- */
    if (ieq(m, "push")) {
        if (st->nops != 1) { fail(&r, "push requiere un operando"); return r; }
        if (a->kind == OPND_REG) { emit(&r, (unsigned char)(0x50 + regcode(a->reg))); }
        else if (a->kind == OPND_IMM) { emit(&r, 0x68); emit_imm32(&r, a); }
        else if (a->kind == OPND_MEM) { emit(&r, 0xFF); emit_modrm(&r, 6, a); }
        else fail(&r, "operando invalido para push");
        return r;
    }
    if (ieq(m, "pop")) {
        if (st->nops != 1) { fail(&r, "pop requiere un operando"); return r; }
        if (a->kind == OPND_REG) { emit(&r, (unsigned char)(0x58 + regcode(a->reg))); }
        else if (a->kind == OPND_MEM) { emit(&r, 0x8F); emit_modrm(&r, 0, a); }
        else fail(&r, "operando invalido para pop");
        return r;
    }

    /* -------- grupo unario (inc/dec/neg/not/mul/div/idiv/imul-1op) -------- */
    {
        unsigned op; int digit;
        if (unary_group(m, &op, &digit)) {
            /* imul con dos operandos se maneja mas abajo */
            if (ieq(m, "imul") && st->nops == 2) {
                /* imul r32, r/m32  ->  0F AF /r */
                if (a->kind != OPND_REG) { fail(&r, "imul: el destino debe ser registro"); return r; }
                emit(&r, 0x0F); emit(&r, 0xAF);
                emit_modrm(&r, regcode(a->reg), b);
                return r;
            }
            if (st->nops != 1) { fail(&r, "esta instruccion requiere un operando r/m"); return r; }
            if (a->kind != OPND_REG && a->kind != OPND_MEM) { fail(&r, "operando r/m invalido"); return r; }
            emit(&r, (unsigned char)op);
            emit_modrm(&r, digit, a);
            return r;
        }
    }

    /* -------- lea r32, [mem] -------- */
    if (ieq(m, "lea")) {
        if (st->nops != 2 || a->kind != OPND_REG || b->kind != OPND_MEM) {
            fail(&r, "lea requiere: registro, memoria"); return r;
        }
        emit(&r, 0x8D);
        emit_modrm(&r, regcode(a->reg), b);
        return r;
    }

    /* -------- movzx r32, r/m (simplificado: fuente de 8 bits) -------- */
    if (ieq(m, "movzx")) {
        if (st->nops != 2 || a->kind != OPND_REG ||
            (b->kind != OPND_REG && b->kind != OPND_MEM)) {
            fail(&r, "movzx requiere: registro, r/m"); return r;
        }
        emit(&r, 0x0F); emit(&r, 0xB6);
        emit_modrm(&r, regcode(a->reg), b);
        return r;
    }

    /* -------- mov -------- */
    if (ieq(m, "mov")) {
        if (st->nops != 2) { fail(&r, "mov requiere dos operandos"); return r; }

        if (a->kind == OPND_REG && b->kind == OPND_IMM) {
            emit(&r, (unsigned char)(0xB8 + regcode(a->reg)));
            emit_imm32(&r, b);
        } else if (a->kind == OPND_REG && (b->kind == OPND_REG || b->kind == OPND_MEM)) {
            emit(&r, 0x8B);                       /* mov r32, r/m32           */
            emit_modrm(&r, regcode(a->reg), b);
        } else if (a->kind == OPND_MEM && b->kind == OPND_REG) {
            emit(&r, 0x89);                       /* mov r/m32, r32           */
            emit_modrm(&r, regcode(b->reg), a);
        } else if (a->kind == OPND_MEM && b->kind == OPND_IMM) {
            emit(&r, 0xC7);                       /* mov r/m32, imm32  (/0)   */
            emit_modrm(&r, 0, a);
            emit_imm32(&r, b);
        } else {
            fail(&r, "combinacion de operandos no soportada para mov");
        }
        return r;
    }

    /* -------- ALU de dos operandos (add/sub/and/or/xor/cmp) -------- */
    {
        const AluOp *alu = find_alu(m);
        if (alu) {
            if (st->nops != 2) { fail(&r, "esta operacion requiere dos operandos"); return r; }

            if (b->kind == OPND_IMM) {
                emit(&r, 0x81);                   /* grupo inmediato /digit    */
                emit_modrm(&r, alu->digit, a);
                emit_imm32(&r, b);
            } else if (a->kind == OPND_REG && (b->kind == OPND_REG || b->kind == OPND_MEM)) {
                emit(&r, (unsigned char)alu->op_r_rm);
                emit_modrm(&r, regcode(a->reg), b);
            } else if (a->kind == OPND_MEM && b->kind == OPND_REG) {
                emit(&r, (unsigned char)alu->op_rm_r);
                emit_modrm(&r, regcode(b->reg), a);
            } else {
                fail(&r, "combinacion de operandos no soportada");
            }
            return r;
        }
    }

    snprintf(r.err, sizeof r.err, "mnemonico no soportado: '%s'", m);
    r.ok = 0;
    return r;
}
