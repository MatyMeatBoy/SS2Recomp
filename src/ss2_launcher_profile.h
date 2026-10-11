#pragma once
#include <cstdint>
#include <cstddef>
struct RecompLauncherCGameInfo;
struct RecompLauncherCNetplayLaunch;
namespace gbarecomp { struct GbaNetplayLaunch; struct GbaNetplayBoot; }
namespace ss2 {
extern bool launcher_recovery_selected;
int prepare_recovery_launch(int players, int seat, uint32_t session, int port,
    const char* host, RecompLauncherCNetplayLaunch* out, char* error, size_t error_cap);
void configure_launcher_profile(RecompLauncherCGameInfo& info);
int supervise_launcher_recovery(const gbarecomp::GbaNetplayLaunch&, const gbarecomp::GbaNetplayBoot&);
}
