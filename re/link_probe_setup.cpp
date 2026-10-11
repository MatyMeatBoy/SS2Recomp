#include "multiplayer_session.h"
#include "ss2_flash_dispatch.h"

void gba_link_probe_setup(gbarecomp::GbaInstance& instance) {
    instance.execution.ram_dispatch = &ss2::flash_stack_dispatch;
    // Offline navigation can omit host pixels, then restore the resulting
    // state in a normal probe for visual verification. Guest PPU timing stays on.
    if (std::getenv("SS2_PROBE_NO_RASTER"))
        instance.ppu.set_rasterization_enabled(false);
}
