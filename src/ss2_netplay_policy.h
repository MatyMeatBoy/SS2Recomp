#pragma once
#include "multiplayer_launch.h"
#include <algorithm>
namespace ss2 {
// Host policy, independent of guest state: a future decomp can reuse the
// confirmed-input admission contract without copying hardware snapshots.
inline void apply_safe_netplay_policy(gbarecomp::GbaNetplayLaunch& launch) {
    launch.rollback = false;
    launch.input_delay = std::max(launch.input_delay, 6u);
    launch.connection_grace_ms = 0;
}
}
