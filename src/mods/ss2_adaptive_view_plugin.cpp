#include "mod_runtime.h"
#include "../ss2_extended_view.h"
#include <cstdlib>

namespace {

void reset_adaptive_view() {
    (void)gba_mod_set_adaptive_view_enabled(0);
    (void)gba_mod_set_view_width(0);
    ss2::enable_equipment_widescreen(false);
}

void activate_adaptive_view() {
    (void)gba_mod_set_adaptive_view_enabled(1);
    ss2::enable_equipment_widescreen(true);
    // Start wide in windowed/headless mode too; fullscreen can adapt further.
    int width = 284;
    // Honor the framework's documented fixed-width override for captures and
    // initial window geometry; normal live resizing still follows the screen.
    if (const char* value = std::getenv("GBARECOMP_VIEW_WIDTH")) {
        char* end = nullptr;
        const long requested = std::strtol(value, &end, 10);
        if (end != value && !*end && requested >= 240 && requested <= 480)
            width = static_cast<int>(requested);
    }
    (void)gba_mod_set_view_width(width);
}

}  // namespace

GBA_MOD_CONSTRUCTOR(ss2_register_adaptive_view_plugin) {
    (void)gba_mod_register_reset_callback(reset_adaptive_view);
    (void)gba_mod_register_activation_plugin(
        "shining-soul-2.adaptive-view", activate_adaptive_view);
}
