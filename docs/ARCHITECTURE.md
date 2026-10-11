# Arquitectura

ShiningSoul2Recomp separa el comportamiento del juego de las mejoras de
presentación y del transporte de red.

```text
ROM + BIOS propios
        │
        ▼
 gba_recompile ──► generated/ ──► ShiningSoul2Recomp.exe
        │                              │
        │                              ├─ PPU nativo 240x160
        │                              ├─ vista adaptativa 16:9
        │                              └─ launcher recomp-ui
        │
        └────────────► runtime GBA + link-cable session
                                      │
                         ┌────────────┴────────────┐
                         ▼                         ▼
                    delay-sync                  rollback
                         │                         │
                         └──── inputs + snapshots ─┘
```

## Vista adaptativa

El juego sigue simulando el hardware GBA a 240x160. Cuando la ventana tiene
espacio adicional, `src/ss2_extended_view.cpp` traduce las columnas laterales
del renderizado a los mapas authored del juego. La escena del estado 4 usa
tres mapas de 90x40 tiles referenciados desde ROM; el proveedor valida sus
dimensiones, límites y coincidencia con el anillo VRAM antes de dibujarlos.

El HUD se conserva en tamaño nativo y sus paneles se anclan a los bordes. Los
menús y las cinemáticas sin un mapa compatible se mantienen centrados para no
inventar contenido.

## Netplay

Cada participante simula todas las máquinas del cable. La red intercambia
entradas, confirmaciones, snapshots y mensajes de control; no transmite
frames de vídeo. La identidad de la sesión incluye ROM, BIOS, build, hardware
de guardado y asignación de puertos.

Con dos jugadores se puede usar rollback o delay-sync. Con tres y cuatro el
modo recomendado es delay-sync porque limita el coste de resimulación; rollback
está disponible para pruebas y sesiones experimentales. Los checkpoints
emparejados se escriben solamente cuando todos los asientos confirman el mismo
tick.

## Código generado y trazabilidad

El contenido de `generated/` se produce localmente desde la ROM verificada y no
se versiona. Las correcciones de runtime viven en el submódulo gbarecomp o en
los archivos de integración del juego. Los scripts bajo `re/` reproducen las
pruebas de vista y transporte sin sonido y con partidas separadas.
