# Recuperación automática — v0.2.1 (experimental)

En Windows, `--netplay-safe` con direcciones directas activa TCP recuperable
para 2–4 jugadores. El UDP de recomp-net permanece disponible iniciando el
juego normalmente. Las salas con relay/Discord conservan el transporte previo:
esta recuperación no se aplica a ese servicio.

## Inicio

1. Ejecuta `build/ShiningSoul2Recomp.exe` y selecciona tu ROM y BIOS propios.
2. Abre **Netplay (experimental)**. El host selecciona **Anfitrión** y crea sala;
   cada invitado introduce su IP de Hamachi y elige un asiento diferente.
3. Selecciona 2–4 participantes; todos deben indicar la misma cantidad, puerto
   TCP (19740 por defecto) e ID de sala (24681 por defecto).
4. Usa el mismo ID en todos. Una sesión nueva inicia otra partida;
   reutilizar la sesión existente recupera su último estado confirmado.

Hamachi es opcional: sirve una LAN o VPN que permita TCP entre los PCs. Si usas
IP pública, hace falta encaminamiento y permiso de firewall para el puerto TCP.
El diseño está destinado a participantes conocidos en LAN/VPN; no incorpora
TLS ni autorización de la primera entrada mediante un servicio público.

## Qué recupera

- Un corte de Wi-Fi: espera, reconecta cada segundo y reinstala el estado común.
- Reinicio de un invitado: conserva su credencial local de asiento y recibe el
  estado del host; sus saves individuales antiguos no reemplazan la partida.
- Reinicio del host: recupera su checkpoint y reproduce el diario de controles
  confirmados, verificando cada digest, antes de reunir otra vez a los invitados.
- Crash de Windows del ejecutable: el launcher inicia un supervisor interno que lo relanza.
  Un cierre normal o un error de validación no provoca un bucle de reinicios.
- Divergencia transitoria: vuelve al estado confirmado del host y espera una
  barrera de digests iguales. Tres reparaciones fallidas en el mismo tick
  detienen la sesión con error en vez de avanzar con estados distintos.

El juego puede pausarse durante cortes y confirmaciones. No se promete ausencia
de lag ni rollback imperceptible. Todos siguen ejecutando todas las consolas
con su cable; la red controla la admisión de ticks, no enchufa/desenchufa el cable
in-game. Se descarta como máximo la transición tentativa todavía no confirmada.

## Estado duradero y autoridad

La autoridad es el host original. Cada transición usa controles de todos los
asientos; todos ejecutan el frame y envían el digest de su estado completo.
El host publica COMMIT sólo después de digests iguales y de escribir el registro
correspondiente con flush de disco. Cada 120 ticks reemplaza atómicamente el
checkpoint de todas las consolas y cable; el diario permite reconstruir los
frames posteriores. Una escritura final incompleta se descarta; corrupción de
registros completos, imágenes o identidad de build produce un error explícito.

La recuperación conserva la sala, imágenes verificadas, configuración de
consolas y generación de conexión. Los paquetes de una generación anterior
no pueden convertirse en controles nuevos. La credencial privada de un asiento
impide que otro proceso lo suplante tras su primera entrada; no debe compartirse.
Los archivos quedan en `build/netplay/recovery/room-ID/seat-N` y no se distribuyen
ni se suben al repositorio. Mantén esa carpeta para recuperar la sesión.

## Límites

El host debe volver en el mismo PC, con sus archivos y una dirección alcanzable.
No hay migración automática a otro PC ni descubrimiento de una nueva IP fija.
Si se usa un nombre DNS, se resuelve otra vez al reconectar. Todos necesitan la
misma build, ROM y BIOS; la aplicación rechaza snapshots de otra identidad.
La igualdad de estados con este protocolo no demuestra gameplay sin bugs ni
rendimiento adecuado entre casas. Las pruebas de juego actuales usan selección
de clases y arranque; falta combate prolongado y una prueba real por Hamachi.

## Fronteras para una futura decomp

`src/ss2_recovery.h` separa transporte/persistencia de la simulación mediante
capture, restore, advance y controles por tick. El adaptador GBA está en
`src/ss2_recovery_runner.cpp`. Una decomp puede cambiar ese adaptador conservando
las barreras, autoridad e identidad; no se promete compatibilidad de snapshots.
Widescreen, LUT de color, ventanas y sonido local quedan fuera de la autoridad.

Referencias: [Mario Kart Recomp](https://github.com/mstan/MarioKartSuperCircuitRecomp/releases),
[psxrecomp](https://github.com/RetroPortingToolKit/psxrecomp/blob/master/docs/NETPLAY.md)
y [GGPO API](https://github.com/pond3r/ggpo/blob/master/src/include/ggponet.h).
Se usaron para separar admisión, estado y liveness; el protocolo TCP de esta
release es propio y no reemplaza el modo UDP/rollback experimental.
