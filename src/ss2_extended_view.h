#pragma once

#include <cstdint>

#include "multiplayer_view.h"
#include "runtime.h"

namespace ss2 {
void enable_equipment_widescreen(bool enabled);

// Adaptive horizontal view for Shining Soul II. Native 240x160 rendering is
// unchanged until a wider view is authorized and installed.
void configure_run_options(gbarecomp::RunOptions& opts);
gbarecomp::GbaNetplayViewPolicy netplay_view_policy();

void install_extended_view(std::uint32_t extra_left,
                           std::uint32_t extra_right);
void reset_extended_view();

}  // namespace ss2
