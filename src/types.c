/* ============================================================================
 * types.c  -  Tablas de nombres de registros, secciones y tipos
 * ==========================================================================*/
#include <string.h>
#include <ctype.h>
#include "types.h"

/* Indexado por el codigo de registro (0..7). */
static const char *kRegNames[8] = {
    "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"
};

const char *reg_name(Register r) {
    if (r < 0 || r > 7) return "?";
    return kRegNames[r];
}

/* Comparacion sin distinguir mayusculas/minusculas. */
static int ieq(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == *b;
}

Register reg_from_name(const char *s) {
    for (int i = 0; i < 8; i++)
        if (ieq(s, kRegNames[i])) return (Register)i;
    return REG_NONE;
}

const char *section_name(SectionId s) {
    switch (s) {
        case SEC_TEXT: return ".text";
        case SEC_DATA: return ".data";
        case SEC_BSS:  return ".bss";
        default:       return "(none)";
    }
}

SectionId section_from_name(const char *s) {
    if (ieq(s, ".text")) return SEC_TEXT;
    if (ieq(s, ".data")) return SEC_DATA;
    if (ieq(s, ".bss"))  return SEC_BSS;
    return SEC_NONE;
}

const char *reloc_kind_name(RelocKind k) {
    return (k == REL_ABS32) ? "ABS32" : "REL32";
}

const char *sym_kind_name(SymKind k) {
    switch (k) {
        case SYM_LABEL:  return "LABEL";
        case SYM_CONST:  return "CONST";
        case SYM_EXTERN: return "EXTERN";
        default:         return "UNDEF";
    }
}
