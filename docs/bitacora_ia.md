Bitácora de uso de IA

Materia:Estructura y Programación de Computadoras. 2026-2
Equipo: 


1. Herramienta de IA utilizada

Herramienta: asistente de IA basado en un modelo de lenguaje (Claude de Anthropic).
Uso: generación de una primera versión del código del proyecto a partir de la 
especificación de la práctica, y apoyo para explicar decisiones de codificación 
(opcodes, ModRM/SIB, formato objeto, relocaciones).

---

## 2. Prompts utilizados (resumen)

Se trabajó principalmente con un prompt extenso que pedía construir el proyecto
completo a partir del documento de la práctica. Las ideas centrales de los
prompts fueron:

1. "Genera un ensamblador de IA-32 en C, con analizador léxico y sintáctico
   **manuales** (sin lex/yacc), que soporte el subconjunto de instrucciones,
   registros y modos de direccionamiento del documento."
2. "Implementa **dos versiones**: una de una pasada (con *fixups* para
   referencias adelantadas) y otra de dos pasadas (tabla de símbolos primero)."
3. "Define un **formato de archivo objeto** con tablas de símbolos, secciones y
   relocaciones, y genera tablas `.txt` legibles."
4. "Implementa un **mini enlazador** que combine varios módulos, resuelva
   `extern`/`global` y aplique relocaciones `ABS32`/`REL32`."
5. "Explica las decisiones de codificación (por qué cada opcode, cómo se arma
   ModRM y SIB)."

> **[EJEMPLO — adaptar]** Si usaron prompts adicionales (por ejemplo, para
> corregir un error concreto o para entender una parte), agréguenlos aquí
> textualmente.

---

## 3. Qué se generó con apoyo de IA

- Estructura inicial del repositorio y `Makefile`.
- Primera versión de todos los módulos en C (`lexer`, `parser`, `symtab`,
  `encoder`, `objfile`, `asmcore`) y de los tres `main`
  (`assembler1`, `assembler2`, `minilinker`).
- Archivos de ejemplo y casos de prueba iniciales.
- Borrador de esta documentación.

---

## 4. Modificaciones y revisión del equipo

> **[EJEMPLO — adaptar]** Documenten aquí lo que realmente hicieron sobre el
> código generado. Algunos ejemplos del tipo de entrada que se espera:

- **[EJEMPLO]** Revisamos `parse_mem` en `parser.c` para entender cómo se
  distingue base de índice, y agregamos comentarios propios / renombramos
  variables para que nos quedara claro.
- **[EJEMPLO]** Añadimos el caso de prueba `tests/test_xxx.asm` para verificar
  el comportamiento de `____`.
- **[EJEMPLO]** Cambiamos `TEXT_BASE` en `minilinker.c` de `0x00000000` a
  `0x____` para ver cómo afectaba a las direcciones del mapa.

Si **no** modificaron una parte, también es válido declararlo: por ejemplo,
"el módulo `util.c` se usó tal cual se generó".

---

## 5. Errores encontrados y cómo se resolvieron

> **[EJEMPLO — adaptar]** Esta sección debe reflejar errores **reales** que
> hayan visto al compilar/ejecutar/probar. Ejemplos del formato esperado:

- **[EJEMPLO]** Al compilar por primera vez aparecieron advertencias por
  faltar `#include <stdlib.h>` en algunos archivos; se agregó el include y
  desaparecieron.
- **[EJEMPLO]** Al ensamblar `____.asm` el resultado no era el esperado en
  `____`; revisamos la tabla `____.txt`, identificamos que `____` y lo
  corregimos / lo entendimos así.
- **[EJEMPLO]** Verificamos a mano la codificación de `mov ebx, eax` (`8B D8`)
  para confirmar que el byte ModRM era correcto.

Si durante su revisión **no** encontraron errores en alguna parte, decláralo
con honestidad (p. ej. "los casos de prueba pasaron sin cambios").

---

## 6. Aprendizajes

> **[EJEMPLO — adaptar]** Una o dos frases por integrante sobre qué entendieron
> mejor gracias al proyecto (por ejemplo, cómo funciona ModRM/SIB, por qué hace
> falta una segunda pasada, qué es una relocación, etc.).

---

## 7. Declaración de integridad

El equipo declara que:

- el uso de IA se limitó a lo descrito en este documento;
- se revisó y comprendió el código entregado;
- las pruebas se ejecutaron y los resultados reportados son reales;
- cualquier afirmación de esta bitácora que no corresponda a lo realizado fue
  corregida antes de la entrega.


