# Conservación de la sesión de cable

El perfil `--netplay-safe` usa entradas confirmadas, retraso mínimo de seis
ticks y espera sin caducidad por silencio de red en una sesión establecida.
Todos los participantes deben elegir el mismo perfil. El rollback sigue
experimental y permanece disponible iniciando el juego normalmente.

El scheduler espera los controles requeridos mientras continúa atendiendo UDP.
No avanza CPU, temporizadores ni cable con controles inventados. Un corte puede
congelar la imagen: conservar la igualdad no elimina la latencia. Salir
explícitamente termina la sesión. Arranque y acuerdos de checkpoints mantienen
sus plazos acotados.

## Fronteras para una futura decomp

- Política: `src/ss2_netplay_policy.h`, separada de hardware y transporte.
- Admisión: controles confirmados para cada tick y asiento.
- Estado: todas las máquinas y cable juntos, nunca saves independientes.
- Verificación: estados confirmados, no imágenes ni estado especulado.
- Presentación: widescreen y ventanas fuera de la autoridad de inputs.

Reemplazar las máquinas por una decomp exige conservar estas fronteras y medir
su determinismo. No garantiza compatibilidad con snapshots antiguos.

## Recuperación pendiente

El perfil tolera silencio temporal mientras los procesos y el mismo transporte
siguen vivos. No implementa reentrada de un proceso nuevo, cambio de dirección,
migración del hub ni reparación automática de un estado divergente. Eso exige
identidad y generación de sesión, autenticación del asiento, checkpoint
acordado, historial de controles y barrera de reanudación común. Una mayoría de
hashes no demuestra por sí sola cuál estado es correcto.

## Referencias primarias

- [Mario Kart Super Circuit Recomp](https://github.com/mstan/MarioKartSuperCircuitRecomp/releases): referencia GBA de cable local replicado, rollback y archivos conjuntos. Su validación publicada de dos jugadores no prueba cuatro en Shining Soul II.
- [psxrecomp NETPLAY](https://github.com/RetroPortingToolKit/psxrecomp/blob/master/docs/NETPLAY.md): separación entre admisión confirmada, digest y presentación; orientación arquitectónica, no código trasplantado.
- [GGPO API](https://github.com/pond3r/ggpo/blob/master/src/include/ggponet.h): distingue interrupción, reanudación y desconexión y permite desactivar el timeout automático. Se conserva esa separación de política sin reemplazar recomp-net.

Pruebas concretas y límites: [NETPLAY_VALIDATION.md](NETPLAY_VALIDATION.md).
