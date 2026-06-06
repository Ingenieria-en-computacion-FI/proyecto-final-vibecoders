#!/usr/bin/env bash
# Limpia binarios y archivos generados.
cd "$(dirname "$0")/.."
make clean
echo ">> Proyecto limpio."
