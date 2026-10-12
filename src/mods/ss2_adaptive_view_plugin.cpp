#include "mod_runtime.h"
#include "../ss2_extended_view.h"

namespace {
void activate_adaptive_view();

void reset_adaptive_view() {
    (void)gba_mod_set_adaptive_view_enabled(0);
    (void)gba_mod_set_view_width(0);
    ss2::enable_equipment_widescreen(false);
}

void activate_equipment_view() {
    activate_adaptive_view();
    ss2::enable_equipment_widescreen(true);
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
    (void)gba_mod_register_activation_plugin(
        "shining-soul-2.equipment-view", activate_equipment_view);
}
