# Validación del netplay — 2026-10-11

El netplay es técnicamente viable, pero sigue en fase experimental. Estas
pruebas arrancan procesos independientes con ROM y BIOS propios, intercambian
entradas por UDP y comparan los archivos `.paired` completos. Usan vídeo y
audio dummy, guardados nuevos y entradas neutras. No recorren una partida
multijugador activa ni prueban Hamachi entre PCs.

## Resultados de esta revisión

La latencia indicada es por trayecto del proxy, no RTT. En la topología de
3–4 jugadores, dos invitados se comunican a través del host y suman trayectos.

| Jugadores | Modo | Ticks | Latencia / jitter / pérdida | Resultado |
| --- | --- | --- | --- | --- |
| 2 | Rollback | 360 | 80 ms / ±25 ms / 2 % | Checkpoints idénticos, salidas 0 |
| 3 | Delay-sync | 360 | 45 ms / ±15 ms / 1,5 % | Checkpoints idénticos, salidas 0 |
| 4 | Rollback | 360 | 45 ms / ±15 ms / 1,5 % | Checkpoints idénticos, salidas 0 |
| 4 | Delay-sync | 600 | 45 ms / ±15 ms / 1,5 % | Checkpoints idénticos, salidas 0 |
| 4 | Delay-sync | Objetivo 600 | Igual, corte del invitado 1 durante 2 s a los 6 s | Sin checkpoint al agotar 60 s |

La prueba sin corte terminó en 26,06 s. La prueba con corte dejó todos los
procesos abiertos hasta el límite de 60 s y fue terminada por el script. Es
una regresión reproducible pendiente de localizar; todavía no prueba el punto
exacto del bloqueo ni garantiza que se haya agotado el período de reconexión.

Los modos rollback exitosos reportaron `netplay replay=0`: la predicción de
entradas neutras no necesitó correcciones. Esto **no demuestra** rollback
efectivo ante entradas tardías distintas, recuperación de una divergencia,
fluidez percibida ni ausencia de lag. El tiempo total incluye arranque,
transferencias de estado, ejecución y checkpoint; tampoco mide FPS de juego.

## Corrección aplicada

La rutina Flash copiada a EWRAM en `02001688 Thumb` podía fallar en netplay.
El callback global del jugador individual no se propagaba a los contextos
de ejecución de las consolas de la sesión. `setup_netplay_instance` instala
ahora el mismo dispatcher de copias verificadas en cada `GbaInstance` usando
la API de preparación del framework. La prueba de 2 jugadores falló antes
del cambio y pasó después. No se modificó código generado ni se agregó HLE.

Los logs de estas ejecuciones multijugador no emiten el resumen de cobertura
del runner individual. Por ello no se atribuye `FULLY_STATIC` a estas pruebas
basándose en un banner ausente.

## Recursos y reproducción

Durante la prueba de cuatro procesos se observaron unos 81 MiB de working set
por proceso (unos 324 MiB entre los cuatro). Es una muestra de esta escena,
no un máximo durante gameplay o rollback. Las pruebas fueron silenciosas y
sin ventanas visibles.

```text
py -3.12 re/test_netplay_local.py --players 2 --mode rollback --frames 360 --latency-ms 80 --jitter-ms 25 --loss 0.02 --timeout 60
py -3.12 re/test_netplay_local.py --players 3 --mode delay-sync --frames 360 --latency-ms 45 --jitter-ms 15 --loss 0.015 --timeout 60
py -3.12 re/test_netplay_local.py --players 4 --mode rollback --frames 360 --latency-ms 45 --jitter-ms 15 --loss 0.015 --timeout 60
py -3.12 re/test_netplay_local.py --players 4 --mode delay-sync --frames 600 --latency-ms 45 --jitter-ms 15 --loss 0.015 --timeout 60
py -3.12 re/test_netplay_local.py --players 4 --mode delay-sync --frames 600 --latency-ms 45 --jitter-ms 15 --loss 0.015 --blackout-at 6 --blackout-seconds 2 --timeout 60
```

Los resultados, logs y checkpoints quedan bajo `re/run/`, excluido de Git.
El script registra la identidad SHA-256 del ejecutable y los ticks de replay.

## Pendiente para considerarlo estable

- Entradas diferentes por jugador dentro de una partida enlazada activa,
  provocando correcciones de rollback y verificando el estado confirmado.
- Recuperación tras corte de red, caída de un proceso y caída del host.
- Reanudar checkpoints en nuevas sesiones de 2–4 jugadores.
- Sesión prolongada entre PCs por VPN y por el relay del launcher.
- Medición de FPS, retraso de controles, tráfico y RAM durante gameplay.

La base incluye lobby y relay, pero su presencia en el código no confirma
disponibilidad del servicio público ni integración operativa de Discord.
En modo directo de 3–4 jugadores, seat 0 es el hub: una caída del host
interrumpe esa ruta y no se ha validado migración automática.
