# v0.2.1 — Netplay experimental dentro del launcher

Esta versión reúne la configuración LAN/Hamachi y la recuperación automática
en el botón **Netplay (experimental)**. No hace falta ejecutar BAT de anfitrión,
invitado ni Online Seguro. El botón **NETPLAY** conserva las salas de recomp-net
y el inicio de sesión con Discord; ese transporte sigue siendo independiente
del nuevo perfil recuperable.

## Para jugar hoy

1. Todos descargan y extraen el ZIP completo, usando la misma versión y sus
   propias ROM europea y BIOS. Ejecutan `build/ShiningSoul2Recomp.exe` o el acceso
   `Jugar ShiningSoul2Recomp.bat`.
2. Para Hamachi, todos entran a la misma red privada y permiten el programa en
   el firewall de esa red. Abren **Netplay (experimental)**.
3. El anfitrión elige la cantidad de jugadores, **Anfitrión (jugador 1)** y
   **Crear sala**. Comparte su IPv4 de Hamachi, ID de sala y puerto TCP.
4. Cada invitado elige la misma cantidad de jugadores, sala y puerto, su asiento
   distinto e introduce la IP del anfitrión. Pulsa **Conectar**.
5. Una vez presentes todos, entran al multijugador dentro del juego y aceptan.

Los valores iniciales son sala **24681** y puerto **19740**. Para empezar otra
partida, o si ven un error de identidad tras actualizar, elijan otra sala.
Para retomar una sala, conserven `build/netplay/recovery` y sus credenciales.

## Cambios

- Panel integrado para 2–4 jugadores, asiento, IP/DNS, sala, puerto y vista.
- Vista 16:9 inicialmente seleccionada desde el botón experimental.
- Supervisor interno que vuelve a iniciar la sesión tras un crash de Windows.
  Los cierres normales son voluntarios y no se relanzan.
- Logs del supervisor y del juego junto a los archivos privados de recuperación.
- Portada original solicitada, reemplazando la ilustración anterior.
- El ZIP incluye imágenes, fuentes y avisos del launcher para que funcione en
  otro PC. Se retiran los BAT online separados del paquete y del repositorio.
- Parches de interfaz y runtime conservados en el repositorio para reconstruir.

## Pruebas y límites

Se verifican arranque desde el launcher oculto, recuperación tras crash y
comparación de estados completos. La evidencia y reproducción están en
[NETPLAY_VALIDATION.md](https://github.com/MatyMeatBoy/SS2Recomp/blob/v0.2.1/docs/NETPLAY_VALIDATION.md).

El modo experimental es **TCP directo para LAN/VPN/Hamachi**. El botón NETPLAY
utiliza el transporte anterior para las salas de Internet: no hereda esta
recuperación. No se ha validado una partida entre casas ni Discord en vivo.
Durante un corte se pausa; una conexión lenta puede generar retraso. El host
original debe volver; todavía no hay migración automática del host.

ROM, BIOS, partidas, estados y credenciales no se incluyen. Las licencias de
dependencias y recursos se distribuyen con el paquete; gbarecomp conserva
PolyForm Noncommercial 1.0.0. No se atribuye FULLY_STATIC a estas pruebas
multijugador sin un reporte de cobertura correspondiente.
