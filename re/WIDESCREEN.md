# Widescreen: comprobación del estado 4 (2026-10-11)

El ejecutable actualizado está en `build/ShiningSoul2Recomp.exe`.
El BAT de la raíz sigue abriendo ese ejecutable. El mod se activa por defecto
para instalaciones nuevas y comienza en 284x160, la aproximación con píxeles
enteros a 16:9 con 160 líneas. En pantalla completa adapta el ancho al monitor.

## Causa y corrección

El proveedor aceptaba solamente mapas descomprimidos en EWRAM. El castillo del
estado 4 usa mapas sin comprimir en ROM. Además, faltaba declarar
`RunOptions::max_view_width`, necesario para vistas anchas fijas/headless.

Las descripciones IWRAM en 0x03003188, con stride 24, contienen estos mapas:

| Capa | Dirección ROM | Tamaño | Cámara comprobada |
| --- | --- | --- | --- |
| BG1 | 0x08634B18 | 90x40 tiles | (239,33) |
| BG2 | 0x08632EF8 | 90x40 tiles | (239,33) |
| BG3 | 0x086312D8 | 90x40 tiles | (239,33) |

Cada mapa coincidió con 49/49 muestras distribuidas del anillo VRAM visible.
El proveedor valida alineación, dimensiones y límites antes de leer RAM/ROM.
No altera el estado del juego; los laterales usan datos del mapa original.
La caché de escena se invalida al comenzar cada frame, también tras restaurar
estados. Los paneles del HUD conservan su tamaño y se anclan a los bordes.

## Pruebas reproducibles

- `py -3.12 re/ws4_probe.py`: carga el estado mediante TCP y guarda volcados.
- `py -3.12 re/test_widescreen.py`: pruebas sin ventana ni sonido, guardados
  separados en `re/run`. Mantiene izquierda/derecha durante 180 frames desde
  el estado 4; carga también los estados 1, 2 y 3 durante 120 frames cada uno.
- Estado 4 inmóvil: 120 frames. Lanzamiento desde la raíz: 30 frames.
- Todas estas ejecuciones finalizaron con FULLY_STATIC y cero dispatch misses.
- Capturas `re/run/ws4-after.png`, `ws-left.png` y `ws-right.png` revisadas:
  escenario adicional real y desplazamiento de cámara en ambos sentidos.

Esto no prueba todo el juego ni una partida de red. Menús/cinemáticas sin un
mapa de campo reconocido siguen centrados con márgenes; no se repite ni estira
el arte para rellenarlos. Los límites físicos del mapa tampoco inventan
terreno adicional. La ampliación de la aparición de actores fuera del área
nativa todavía necesita análisis del código de culling del juego.
