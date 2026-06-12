# ia32-assembler-linker

Ensamblador y enlazador para un **subconjunto de la arquitectura IA-32**,
escrito en C, para la materia **Estructura y Programación de Computadoras
(2026-2)**.

Incluye:

- **`assembler1`** — ensamblador de **una pasada** (con *fixups* para
  referencias adelantadas).
- **`assembler2`** — ensamblador de **dos pasadas** (tabla de símbolos primero).
- **`minilinker`** — **enlazador** que combina varios módulos objeto, resuelve
  símbolos `extern`/`global` y aplica relocaciones.

El analizador léxico y el sintáctico están escritos **a mano** (sin
`lex`/`yacc`/`flex`/`bison`), y no se usa ningún ensamblador ni enlazador
externo.

---

## Inicio rápido

```bash
# 1. Compilar
make

# 2. Ensamblar un programa (una o dos pasadas)
./assembler1 examples/test1.asm build/test1
./assembler2 examples/test1.asm build/test1

# 3. Ensamblar y enlazar dos módulos
./assembler2 examples/mod1.asm build/mod1
./assembler2 examples/mod2.asm build/mod2
./minilinker build/mod1.o build/mod2.o build/program.hex

# 4. Ejecutar todos los casos de prueba
make test
```

---

## Estructura del repositorio

```
ia32-assembler-linker/
├── include/        # cabeceras (.h)
├── src/            # código fuente (.c)
├── tests/          # casos de prueba (.asm)
├── examples/       # programas de ejemplo (.asm)
├── scripts/        # build.sh, run_tests.sh, clean.sh
├── docs/           # documentación
├── build/          # objetos y salidas (se generan al compilar)
└── Makefile
```

---

## Funcionalidad soportada

- **Registros:** `eax, ebx, ecx, edx, esi, edi, ebp, esp`.
- **Instrucciones:** `mov, push, pop, lea, movzx, add, sub, inc, dec, cmp, neg,
  mul, imul, div, idiv, and, or, xor, not, jmp, je, jne, jg, jge, jl, jle,
  call, ret, nop, int`.
- **Directivas:** `section, global, extern, db, dw, dd, resb, resw, resd, equ,
  org`.
- **Modos de direccionamiento:** inmediato, registro, registro-registro,
  `[1000]`, `[ebp+4]`, `[esi+ecx]`, `[ebx+esi+8]`, `[ebx+ecx*4]`,
  `[ebx+ecx*4+32]`, `[etiqueta]` (con SIB y escalas 1/2/4/8).
- **Relocaciones:** `ABS32` y `REL32`, resueltas por el enlazador.

---

## Documentación

- [`Manual de usuario`](docs/manual_usuario.pdf) — Cómo compilar y usar.
- [`Formato de Objeto`](docs/formato_objeto.md) — Formato `.o` y decisiones
  de codificación (opcodes, ModRM, SIB).
- [`Reporte técnico`](docs/reporte_tecnico.md) — Reporte técnico.
- [`Bitácora IA`](docs/bitacora_ia.md) — Bitácora de uso de IA.

---

## Equipo

> Dávalos Carmona Gael Eduardo
> Morales Basilio Alejandra Sofía
> Reyes García Miguel Ángel
> Santos Cruz Lair Abraham
> Torres Rodríguez Lizeth Danae

