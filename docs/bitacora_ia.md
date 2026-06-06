Bitacora de uso de IA.
1. Qué herramientas de IA utilizamos
Se utilizó Claude, de Anthropic, mediante su interfaz web claude.ai, como herramienta de apoyo para la organización del proyecto, generación de una base inicial de código, aclaración de dudas técnicas y redacción de documentación.
No se utilizó ninguna otra herramienta de IA.


3. Qué prompts utilizamos
Se utilizaron indicaciones generales y consultas específicas relacionadas con los requisitos de la práctica. Las peticiones se enfocaron en comprender la estructura del ensamblador, organizar los módulos del proyecto y revisar aspectos técnicos de la implementación.
Algunos de los temas consultados fueron:
Estructura de un ensamblador IA-32 en C sin utilizar lex/yacc ni flex/bison.
Organización de una versión de una pasada y otra de dos pasadas.
Manejo de etiquetas, referencias adelantadas, símbolos externos y relocaciones.
Representación de instrucciones, registros y modos de direccionamiento.
Codificación de instrucciones usando ModRM y SIB.
Diseño de casos de prueba mínimos para validar el funcionamiento.
Documentación del formato de archivo objeto y del proceso de enlace.
Estas consultas se usaron como apoyo para orientar el desarrollo. El equipo revisó, corrigió, probó y ajustó manualmente las partes principales del proyecto.


3. Qué código fue generado automáticamente

La IA se utilizó para generar una base inicial del proyecto, incluyendo algunos archivos fuente, cabeceras, ejemplos, scripts, casos de prueba y borradores de documentación. Estos archivos no se entregaron sin revisión, ya que posteriormente fueron analizados, corregidos y validados por el equipo.

Entre los elementos generados como base inicial estuvieron:

Cabeceras en include/:
types.h, util.h, lexer.h, symtab.h, parser.h, encoder.h, objfile.h, asmcore.h.

Archivos fuente en src/:
types.c, util.c, lexer.c, symtab.c, parser.c, encoder.c, objfile.c, asmcore.c, assembler1.c, assembler2.c, minilinker.c.

Archivos de soporte:
Makefile, README.md, .gitignore.

Ejemplos en examples/:
test1.asm, mod1.asm, mod2.asm.

Casos de prueba en tests/:
test_mov_imm.asm, test_jumps.asm, test_forward.asm, test_extern.asm, test_call.asm, test_reloc.asm, test_sib.asm.

Scripts en scripts/:
build.sh, run_tests.sh, clean.sh.

Documentación en docs/:
formato_objeto.md, manual_usuario.md, reporte_tecnico.md, además de un borrador inicial de esta bitácora.



4. Qué modificaciones realizamos manualmente

Sofía: Revisó lexer.c y parser.c. Verificó la tokenización carácter por carácter, la distinción entre registros e identificadores y el parseo de operandos de memoria con base, índice, escala y desplazamiento. También confirmó que las etiquetas requieren : y que los comentarios con ; se manejan correctamente.
Lizeth: Revisó asmcore.c, assembler1.c y assembler2.c. Trazó el flujo de assemble_stream y verificó cómo define_labels controla la diferencia entre la versión de una pasada y la de dos pasadas. También probó la detección de redefinición de etiquetas y confirmó que se reporta error a stderr con código de salida 1.
Ángel: Revisó encoder.c. Verificó manualmente la codificación de instrucciones contra la referencia Intel, por ejemplo mov ebx, eax → 8B D8 y mov eax, [ebx+ecx*4+32] → 8B 44 8B 20. También comprobó los modos de direccionamiento usados en tests/test_sib.asm.
Lair: Revisó objfile.c y minilinker.c. Trazó el enlace de mod1.o + mod2.o, verificando las relocaciones y comparando la salida generada con build/program.hex.

Los módulos util.c y types.c se usaron prácticamente sin cambios, ya que solo contenían funciones auxiliares y definiciones generales.



5. Qué errores encontramos
Durante la compilación y las pruebas se encontraron los siguientes errores:

Faltaban includes:
gcc reportó advertencias de implicit declaration of function 'free' en symtab.c, asmcore.c, assembler1.c y assembler2.c.
Se corrigió agregando:

#include <stdlib.h>

Indentación engañosa:
En encoder.c, algunas líneas relacionadas con ret y nop tenían if/else y return en la misma línea, lo que generaba la advertencia -Wmisleading-indentation.
Se corrigió separando las instrucciones en distintas líneas para hacer más claro el flujo.
Colisión de nombres en una macro: En minilinker.c, la macro RESOLVE tenía un parámetro llamado name, que entraba en conflicto con el miembro .name de la estructura Symbol.   Esto provocaba errores de compilación. Se corrigió reemplazando la macro por una función llamada resolve_symbol, usando el parámetro want.
Después de estas correcciones, el proyecto compiló sin advertencias.
