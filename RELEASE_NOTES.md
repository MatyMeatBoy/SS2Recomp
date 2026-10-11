# ShiningSoul2Recomp v0.1.1 — 2026-10-11

Release visual y de netplay para Shining Soul II Europe AU2P.

## Incluido

- Ejecutable Windows x64 recompilado estáticamente.
- Launcher basado en recomp-ui para elegir ROM, BIOS, ajustes y mods.
- Vista adaptativa 16:9 en escenas de campo, con mapa real a izquierda y
  derecha del área GBA original.
- HUD y paneles laterales reposicionados sin deformar sprites.
- Compatibilidad de mapas tanto descomprimidos en EWRAM como referenciados
  directamente desde ROM.
- Netplay directo de 2, 3 y 4 jugadores sobre el cable emulado.
- Modo delay-sync para sesiones grandes y modo rollback seleccionable.
- Checkpoints emparejados para guardar y reanudar una sesión completa.
- Validación de identidad de ROM, BIOS, compilación y configuración de cable.
- Pruebas silenciosas reproducibles para vista, transporte, pérdida, jitter y
  rollback.
- Portada propia del launcher con identidad ShiningSoul2Recomp; se eliminaron
  referencias visuales y textuales heredadas de otros proyectos.

## Resultados de validación

Se ejecutaron cuatro procesos independientes en loopback. Cada proceso simuló
cuatro consolas y usó un asiento distinto. En delay-sync y rollback se probaron
240 ticks con 45 ms de latencia, ±15 ms de jitter y 1,5 % de pérdida simulada.
Los cuatro procesos terminaron correctamente y produjeron el mismo checkpoint
emparejado, byte por byte.

La prueba de vista cargó el savestate de una zona con escenario real fuera de
los 240 píxeles nativos. El proveedor verificó tres mapas de 90x40 tiles y una
cámara común; las capturas mostraron contenido distinto al desplazarse a la
izquierda y a la derecha. Los estados de prueba adicionales también terminaron
con `FULLY_STATIC` y cero dispatch misses.

## Limitaciones conocidas

- El BIOS y la ROM deben ser proporcionados por cada usuario.
- El netplay entre PCs reales todavía necesita una sesión externa prolongada;
  la prueba publicada aquí es una simulación de red local con impedimentos.
- Menús, diálogos y cinemáticas que no exponen un mapa de campo compatible se
  muestran centrados dentro de la vista ancha.
- La reconexión y el reanudar desde un checkpoint están implementados en la
  infraestructura, pero requieren validación con una caída real durante una
  partida de cuatro PCs.
- El modo rollback de cuatro jugadores es experimental; delay-sync es el modo
  recomendado para una primera sesión.

## Instalación legal

No se distribuyen ROM, BIOS, saves, savestates ni dumps derivados. Consulta
`README.md` para hashes requeridos y la configuración de red.
