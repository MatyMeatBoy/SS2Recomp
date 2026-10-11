# v0.2.0 — Recuperación automática de salas

Release experimental para Windows x64. Si un jugador pierde la conexión, la sala
pausa los avances confirmados y espera su regreso. Al reconectar, todos recuperan
el mismo estado completo de las consolas y del cable. Los demás jugadores no
necesitan crear otra partida.

## Cambios

- Nuevo perfil directo TCP con `--netplay-safe`, para 2–4 jugadores.
- Confirmación de controles y comparación de estados antes de confirmar cada frame.
- Restauración automática ante una divergencia transitoria; los errores persistentes
  se detienen explícitamente en lugar de continuar partidas diferentes.
- Checkpoints atómicos y diario de controles confirmado en disco: el host original
  puede reiniciarse y reconstruir la sesión, conservando sus archivos.
- Reconexión con credenciales de asiento, comprobación de build, ROM y BIOS, y
  generaciones que descartan mensajes antiguos.
- Los BAT online supervisan el proceso y lo reinician tras un crash de Windows.
  Cerrar la ventana voluntariamente no provoca un reinicio.
- Nuevos `Online Host.bat` y `Online Invitado.bat`, con parámetros de sala,
  número de jugadores y asiento. Colores originales y vista 16:9 sin estirar.

## Uso

Extrae todo el ZIP. Abre primero `Jugar ShiningSoul2Recomp.bat` y selecciona tu
propia ROM europea y BIOS. Para dos jugadores, el anfitrión abre `Online Host.bat`
y el invitado abre `Online Invitado.bat` e introduce la IP alcanzable del host.
Hamachi sirve como red privada; permite el puerto TCP 19740 en el firewall.

Para cuatro jugadores, desde una terminal en la carpeta extraída:

```bat
"Online Host.bat" -Players 4 -Session 24680
"Online Invitado.bat" -Players 4 -Seat 1 -Session 24680 -HostAddress 25.x.x.x
```

Los otros invitados usan `-Seat 2` y `-Seat 3`. Todos deben usar la misma versión,
ROM, BIOS, cantidad de jugadores e ID de sala. Conserva `build/netplay/recovery`;
usa otro `-Session` para empezar una sala nueva. No compartas las credenciales.

## Validación y límites

Pasaron las pruebas locales de reinicio del host (3 consolas), divergencia
provocada (3), reentrada de invitado (4), crash con reinicio automático (2) y
corte TCP de 65 segundos (3). En cada ejecución los estados finales completos
coincidieron entre todos los participantes. Las escenas cubren arranque y
selección de clases con controles diferentes; no combate enlazado prolongado.

Consulta [las pruebas](https://github.com/MatyMeatBoy/SS2Recomp/blob/v0.2.0/docs/NETPLAY_VALIDATION.md) y
[el protocolo](https://github.com/MatyMeatBoy/SS2Recomp/blob/v0.2.0/docs/NETPLAY_RECOVERY.md) para escenarios, resultados y reproducción.
Las pruebas locales no equivalen a una sesión prolongada entre casas.

Este perfil confirma cada frame: una conexión lenta puede introducir retraso;
no se promete rollback imperceptible. El host original debe regresar con sus
archivos y una dirección alcanzable. No hay migración del host a otro PC.
Lobby, relay y Discord conservan el transporte anterior; esta recuperación se
aplica al modo directo. Faltan pruebas de combate prolongado y Hamachi real.

El ZIP contiene ejecutables y configuración pública; no incluye ROM, BIOS,
partidas, savestates ni credenciales. La recompilación y el motor de ejecución
provienen de gbarecomp y sus dependencias; no se afirma cobertura FULLY_STATIC
para estos escenarios multijugador.
Las licencias de dependencias se incluyen en `licenses`; gbarecomp utiliza
PolyForm Noncommercial 1.0.0 y mantiene sus propios términos.
