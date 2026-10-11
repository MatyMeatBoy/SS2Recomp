# Validación del netplay — 2026-10-11

## v0.2.1: integración del launcher

El nuevo binario tiene SHA-256
`30736d328f15f45eb82a9e03ad4a0993f2237c801909b02787fc58873c512b61`.
Las siguientes pruebas arrancaron el launcher oculto, prepararon la sala con
su callback experimental y lanzaron el supervisor interno y el juego:

| Consolas | Escenario | Resultado |
| --- | --- | --- |
| 2 | Crash NTSTATUS de invitado en tick 30; reinicio automático | 120 ticks, estados iguales, 11,20 s |
| 4 | Arranque de cuatro participantes desde el launcher | 120 ticks, estados iguales, 11,20 s |
| 3 | Selección de clases, controles distintos, corte TCP de 12 s | 120 ticks, estados iguales, 32,20 s |
| 2 | ZIP extraído: launcher y supervisor desde el paquete distribuido | 90 ticks, estados iguales, 7,09 s |

También se inspeccionaron capturas del dashboard y del panel experimental:
portada original, botones separados y controles de cuatro asientos visibles.
No se abrieron ventanas visibles ni se reprodujo audio durante las pruebas.
Las pruebas GUI usan el script de diagnóstico para completar la selección;
no representan una sesión manual completa ni una prueba entre casas.

```text
py -3.12 re/test_recovery.py --native --launcher --players 4 --frames 120
py -3.12 re/test_recovery.py --native --launcher --players 2 --frames 120 --at 30 --fault watchdog
```

El resumen público está en `docs/recovery-v0.2.1-results.json`. Se mantienen
los límites de combate prolongado, Discord en vivo y Hamachi remoto pendientes.
El botón experimental no modifica el transporte de las salas recomp-net.

## v0.2.0: perfil TCP recuperable

Pruebas locales con procesos independientes, vídeo/audio dummy y comparación
SHA-256 de los estados completos al terminar. Todos salieron con código 0 y
estados idénticos entre asientos. El binario distribuido probado tiene SHA-256
`afd336e0e07af70a70c944774b59e30bbb00caa3a175659749cccba2ef69666a`.

| Consolas | Escena | Fallo provocado | Ticks | Duración |
| --- | --- | --- | --- | --- |
| 3 | Selección de clases, controles distintos | Terminación y reinicio del host en tick 60 | 180 | 39,17 s |
| 3 | Selección de clases, controles distintos | Corrupción de EWRAM de un invitado en frame 30 | 120 | 29,05 s |
| 4 | Arranque | Terminación y reentrada de un invitado en tick 30 | 120 | 12,80 s |
| 2 | Arranque | Crash NTSTATUS y relanzamiento mediante supervisor PowerShell | 120 | 11,06 s |
| 3 | Selección de clases, controles distintos | Corte TCP de 65 segundos en tick 30 | 120 | 83,83 s |

El proxy del corte contabilizó 10.830.324 bytes acumulados, incluyendo copias
completas de estado durante la entrada y recuperación. No es una medición de
tráfico constante por segundo. Una muestra de procesos durante estas pruebas
registró aproximadamente 76–81 MiB de RAM por instancia; no es un máximo.

La capa de protocolo también pasó pruebas independientes sin ROM: 2 consolas
normales, reinicio de host, divergencia, 4 con reinicio de invitado y 3 con corte
de 65 segundos. Esto verifica transporte/persistencia sin sustituir pruebas GBA.

Reproducción básica, con ROM y BIOS locales propios:

```text
py -3.12 re/test_recovery.py --native --players 4 --frames 120 --at 30 --fault guest
py -3.12 re/test_recovery.py --native --players 2 --frames 120 --at 30 --fault watchdog
py -3.12 re/test_recovery.py --players 3 --frames 180 --at 60 --fault network --cut-seconds 65 --timeout 150
```

Las pruebas de selección de clases requieren un estado nativo de tres consolas
aportado localmente mediante `--initial-state`, y `re/party_stress_inputs.txt`
mediante `--inputs`. No se distribuyen estados derivados del juego. Los resultados
completos quedan en `re/run/`; el resumen público está en
`docs/recovery-v0.2.0-results.json`.

**Pendiente:** combate enlazado prolongado, PCs remotos por Hamachi, latencia real
y rendimiento sostenido. Durante la caída se pausa; no hay migración del host ni
promesa de rollback imperceptible. Estas pruebas no emiten cobertura FULLY_STATIC.

## Historial del transporte UDP

El netplay es técnicamente viable, pero sigue en fase experimental. Estas
pruebas arrancan procesos independientes con ROM y BIOS propios, intercambian
entradas por UDP y comparan los archivos `.paired` completos. Usan vídeo y
audio dummy y entradas neutras o guiones por asiento según la prueba. No recorren una partida
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
| 4 | Perfil seguro | 360 | 45 ms / ±15 ms / 1,5 % | Checkpoints idénticos, salidas 0; 34,70 s |
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

### Revisión posterior con controles diferentes

Se recuperó `PrimatesGBA/test/party.txt`, generado originalmente por
`gen_party.ps1`. Su adaptación `re/party_native_es.txt` espera 720 ticks al
arrancar, frente a 420 en el emulador antiguo. Capturas independientes
confirmaron el menú en español y tres clases distintas. La secuencia aún
requiere adaptación para completar la introducción y verificar combate
enlazado; no se presenta una selección de clases como gameplay multijugador.

`SS2LinkProbe` reutiliza el corpus compilado de la aplicación y las herramientas
`gbarecomp/tools/link_session`. Ejecuta el mismo `GbaMultiplayerSession` y
`GbaNetplayMatch`, con el dispatcher Flash instalado por consola. Su entrada
de prueba cambia botones por asiento cada tres ticks. No automatiza el
launcher, Discord ni la selección de mandos físicos. Los `.paired` de este
probe son snapshots nativos de prueba, **no** archivos compatibles con
`--netplay-resume` del launcher.

| Tres procesos, 180 ticks desde selección de clases | Resultado |
| --- | --- |
| Rollback, loopback sin degradación, delay predeterminado | 1 tick reejecutado en cada proceso; snapshots idénticos |
| Rollback, 45 ms / ±15 ms / 1,5 %, delay predeterminado | Snapshots idénticos; cero replay; no satisface `--require-replay` |
| Rollback, 100 ms / ±60 ms / 2 %, delay solicitado de 2 | 16, 16 y 20 episodios RESIM observados; fallo `checkpoint state digest mismatch` |
| Delay-sync, 100 ms / ±60 ms / 2 %, delay solicitado de 2 | Snapshots idénticos; salidas 0 |
| Delay-sync, 45 ms / ±15 ms / 1,5 %, corte de 2 s | Snapshots idénticos; salidas 0; 14,84 s totales |
| Perfil seguro, 45 ms / ±15 ms / 1,5 %, corte de 65 s | Snapshots idénticos; salidas 0; 79,14 s totales |

El perfil seguro supera el timeout previo de 60 s de una sesión establecida
sin avanzar el cable con entradas inventadas. La prueba larga conserva el
mismo SHA-256 final que la prueba sin pérdida:
`906882bd819c9acd4fa19886651ff2da9245570e33aecac0794ac91a1666a8ca`.
El probe usado tiene SHA-256
`a6668d802d66fd7b50c4dfb88e2c7ab5b72c3f33c4e0dfb78475cadd00a2f723`.

Se observaron 1.461.378 datagramas en la prueba larga, incluidos 861.529
descartados por el corte artificial. El tráfico excesivo requiere investigar
retransmisiones antes de recomendar este perfil por VPN; el resultado confirma
igualdad del estado, no eficiencia ni fluidez de red.
La prueba del ejecutable con cuatro procesos también tuvo tráfico elevado:
1.422.821 datagramas en 34,70 s, incluso sin corte. No parece limitado a la
espera prolongada. Su SHA-256 es
`09d37a14d5cd572b58136d92962c7543c8594dde13caba1d308cf6cdb8ad2384`.

El estrés provoca correcciones reales, pero el rollback no logró acordar
el checkpoint. La primera causa de esa diferencia aún no está localizada.
El fallo se detectó y la herramienta no publicó un guardado divergente como
válido. Durante el estrés se observaron aproximadamente 304–307 MiB por
proceso, alrededor de 917 MiB en total; la cifra de 81 MiB de las pruebas
neutras no describe esta carga.

La prueba local con rollback reportó unos 11,6 FPS de avance; delay-sync con
la red de estrés, unos 6,5 FPS. Ambas ejecutan nueve consolas en el mismo PC
(tres por proceso), además del proxy. No permiten extrapolar rendimiento
entre PCs ni afirmar que sea imperceptible. El online aún requiere correcciones
de estabilidad y mediciones con gameplay real.

Reproducción, usando únicamente estados locales derivados de la ROM propia:

```text
cmake -S . -B build -DSS2_BUILD_LINK_PROBE=ON
cmake --build build --target SS2LinkProbe -j2
py -3.12 re/test_party_recomp.py --stop party_class
```

El último comando imprime la carpeta de resultados. Usa su `party.state`
como `ESTADO_LOCAL` en estas pruebas:

```text
py -3.12 re/test_netplay_local.py --players 3 --mode rollback --frames 180 --probe-state ESTADO_LOCAL --probe-inputs re/party_stress_inputs.txt --timeout 120
py -3.12 re/test_netplay_local.py --players 3 --mode rollback --frames 180 --latency-ms 100 --jitter-ms 60 --loss 0.02 --input-delay 2 --require-replay --probe-state ESTADO_LOCAL --probe-inputs re/party_stress_inputs.txt --timeout 180
py -3.12 re/test_netplay_local.py --players 3 --mode delay-sync --frames 180 --latency-ms 100 --jitter-ms 60 --loss 0.02 --input-delay 2 --probe-state ESTADO_LOCAL --probe-inputs re/party_stress_inputs.txt --timeout 120
py -3.12 re/test_netplay_local.py --safe --players 3 --frames 180 --latency-ms 45 --jitter-ms 15 --loss 0.015 --blackout-at 6 --blackout-seconds 65 --probe-state ESTADO_LOCAL --probe-inputs re/party_stress_inputs.txt --timeout 120
```

El target de prueba requiere MinGW Makefiles y reutiliza los objetos del
ejecutable para ahorrar compilación y RAM. El guion de navegación registra
sus capturas y hashes en `re/run/`. No se incluyen estados, ROM ni BIOS en Git.

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
