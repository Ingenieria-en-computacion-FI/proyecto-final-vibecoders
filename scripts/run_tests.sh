#!/usr/bin/env bash
# ============================================================================
# run_tests.sh  -  Ejecuta los casos de prueba minimos del proyecto.
#
# Para cada archivo tests/*.asm corre el ensamblador de 1 pasada y el de 2
# pasadas; luego ensambla los dos modulos de examples/ y los enlaza.
# Informa OK/FALLA por cada caso y termina con codigo != 0 si algo falla.
# ============================================================================
cd "$(dirname "$0")/.."

BUILD=build
mkdir -p "$BUILD"

# Asegura que los binarios existan.
if [ ! -x ./assembler1 ] || [ ! -x ./assembler2 ] || [ ! -x ./minilinker ]; then
    echo ">> Compilando primero ..."
    make >/dev/null
fi

TMP_ERR=$(mktemp) # Archivo temporal para capturar errores reales del compilador
fallos=0
total=0

run() {            # run <descripcion> <comando...>
    total=$((total+1))
    local linea="$1"; shift
    local desc="$1"; shift
    
    if "$@" >/dev/null 2> "$TMP_ERR"; then
        printf "  [ OK ]  %s\n" "$desc"
    else
        printf "  [FALLA] %s " "$desc"
        # Se muestra la línea y archivo del error arrojado por el ensamblador
        if [ -s "$TMP_ERR" ]; then
            echo "          ↳ Detalle:"
            sed 's/^/            /' "$TMP_ERR" # Agrega sangría al error
        fi
        fallos=$((fallos+1))
    fi
}

echo "============================================================"
echo " Casos de prueba (una pasada / dos pasadas)"
echo "============================================================"
for t in tests/test_*.asm; do
    name=$(basename "$t" .asm)
    # Pasamos $LINENO como primer parámetro a la función run
    run "$LINENO" "assembler1  $name ($t)" ./assembler1 "$t" "$BUILD/${name}_a1"
    run "$LINENO" "assembler2  $name ($t)" ./assembler2 "$t" "$BUILD/${name}_a2"
done

echo
echo "============================================================"
echo " Multiples modulos + enlace (mini linker)"
echo "============================================================"
run "$LINENO" "assembler2  mod1" ./assembler2 examples/mod1.asm "$BUILD/mod1"
run "$LINENO" "assembler2  mod2" ./assembler2 examples/mod2.asm "$BUILD/mod2"
run "$LINENO" "minilinker  mod1+mod2" ./minilinker "$BUILD/mod1.o" "$BUILD/mod2.o" "$BUILD/program.hex"

echo
echo "============================================================"
echo " Programa de ejemplo general (test1)"
echo "============================================================"
run "$LINENO" "assembler1  test1" ./assembler1 examples/test1.asm "$BUILD/test1_a1"
run "$LINENO" "assembler2  test1" ./assembler2 examples/test1.asm "$BUILD/test1_a2"

# Limpiamos el archivo temporal
rm -f "$TMP_ERR"

echo
echo "------------------------------------------------------------"
if [ "$fallos" -eq 0 ]; then
    echo " RESULTADO: $total/$total casos OK"
    echo "------------------------------------------------------------"
    exit 0
else
    echo " RESULTADO: $((total-fallos))/$total OK, $fallos con FALLA"
    echo "------------------------------------------------------------"
    exit 1
fi
