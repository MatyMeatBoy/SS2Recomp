# v0.2.2 — Salas experimentales y conexiones ICE

Crear una sala experimental con un puerto ocupado cerraba el launcher y el
error quedaba en un registro oculto. Ahora se comprueba el puerto antes de salir
del formulario: puedes cambiarlo y reintentar allí mismo. Los fallos posteriores
del supervisor muestran un aviso con los registros.

## LAN / Hamachi

Abre `build/ShiningSoul2Recomp.exe` y **Netplay (experimental)**. El anfitrión
selecciona 2–4 jugadores y crea la sala. Cada invitado usa otro asiento, la misma
sala y puerto, y la IPv4 de Hamachi del anfitrión. Para dos instancias en un mismo
PC, crea solamente un anfitrión; la segunda debe entrar como invitado a
`127.0.0.1`. Si el puerto está ocupado o reservado, elige otro y comunícalo a todos.

La sala inicial cambia a **24683** para separar los archivos de recuperación de
la versión anterior. Conserva los archivos antiguos: esta versión no borra ni
convierte partidas de otra identidad de compilación.

La espera de jugadores y la reconexión muestran un panel en el juego, incluso
en pantalla completa. El perfil sigue siendo TCP con controles confirmados:
pausa mientras falta un participante, conserva checkpoints y admite reentrada.
No promete ausencia de latencia, rollback imperceptible ni migración del host.

## Salas de Discord / recomp-net

Se habilita libjuice estático y se incorpora el traspaso de los agentes ICE
conectados del lobby a la sesión GBA. Antes, el motor intentaba abrir UDP por
direcciones convencionales sin adoptar el transporte indicado por el launcher.
Ahora conserva los agentes hasta que el motor los recibe, valida los asientos y
libera los recursos si no se puede iniciar la sesión.

## Validación

- Dos instancias desde el launcher experimental: 90 ticks, cierre correcto y
  estados finales idénticos.
- Cuatro instancias desde el launcher experimental: 120 ticks, corte de red de
  tres segundos y recuperación con estados finales idénticos.
- Puerto ocupado: error visible en el formulario; el launcher permanece abierto.
- Prueba de biblioteca ICE: tres sesiones adoptan las conexiones de la sala y
  los invitados intercambian datagramas a través del host.
- Lobby online real: dos instancias en el mismo PC, login almacenado, conexión
  ICE, vista 16:9 y checkpoint confirmado idéntico en tick 120.

Estas pruebas breves no equivalen a una sesión larga de combate entre países.
ROM, BIOS, partidas, credenciales y registros privados no forman parte del ZIP.
