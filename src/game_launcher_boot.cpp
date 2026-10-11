#include "ss2_launcher_profile.h"
#define GBARECOMP_CUSTOMIZE_LAUNCHER_GAME_INFO(gi) ss2::configure_launcher_profile(gi)
#include "launcher_seam.h"

#include "game_launcher_boot.h"

int game_launcher_preboot(std::vector<std::string>& args,
                          const gbarecomp::RunOptions& opts) {
    return gbarecomp_launcher_preboot(args, opts);
}
