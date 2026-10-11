#include "mod_runtime.h"

namespace {

void reset_adaptive_view() {
    (void)gba_mod_set_adaptive_view_enabled(0);
    (void)gba_mod_set_view_width(0);
}

void activate_adaptive_view() {
    (void)gba_mod_set_adaptive_view_enabled(1);
    // Start wide in windowed/headless mode too; fullscreen can adapt further.
    (void)gba_mod_set_view_width(284);
}

}  // namespace

GBA_MOD_CONSTRUCTOR(ss2_register_adaptive_view_plugin) {
    (void)gba_mod_register_reset_callback(reset_adaptive_view);
    (void)gba_mod_register_activation_plugin(
        "shining-soul-2.adaptive-view", activate_adaptive_view);
}
