# ============================================================================
# Makefile  -  ia32-assembler-linker
#
#   make            compila assembler1, assembler2 y minilinker
#   make test       compila y ejecuta el script de pruebas
#   make clean      borra binarios y archivos generados en build/
# ============================================================================

CC      := gcc
CFLAGS  := -Wall -Wextra -std=c11 -g -Iinclude
SRCDIR  := src
OBJDIR  := build

# Modulos compartidos por los tres ejecutables.
COMMON      := types util lexer symtab parser encoder objfile asmcore
COMMON_OBJ  := $(addprefix $(OBJDIR)/,$(addsuffix .o,$(COMMON)))

BINS := assembler1 assembler2 minilinker

all: $(BINS)

$(OBJDIR):
	mkdir -p $(OBJDIR)

# Regla generica de compilacion de cada .c a build/*.o
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

assembler1: $(COMMON_OBJ) $(OBJDIR)/assembler1.o
	$(CC) $(CFLAGS) $^ -o $@

assembler2: $(COMMON_OBJ) $(OBJDIR)/assembler2.o
	$(CC) $(CFLAGS) $^ -o $@

minilinker: $(COMMON_OBJ) $(OBJDIR)/minilinker.o
	$(CC) $(CFLAGS) $^ -o $@

test: all
	bash scripts/run_tests.sh

clean:
	rm -f $(OBJDIR)/*.o $(BINS)
	rm -f $(OBJDIR)/*.txt $(OBJDIR)/*.hex

.PHONY: all clean test
