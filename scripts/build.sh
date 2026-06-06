#!/usr/bin/env bash
# Compila el proyecto completo.
set -e
cd "$(dirname "$0")/.."
echo ">> Compilando ia32-assembler-linker ..."
make
echo ">> Listo. Ejecutables: assembler1, assembler2, minilinker"
