#pragma once
#include <array>
namespace gbarecomp { struct GbaNetplayLaunch; struct GbaNetplayBoot; }
namespace ss2 {
struct LocalMultiplayerOptions {
    int players=2, display=-1, layout=0, audio_player=0, deadzone=12000;
    bool integer_scale=false;
    std::array<int,4> controllers{0,1,2,3};
};
inline bool launcher_local_selected=false;
inline LocalMultiplayerOptions local_multiplayer_options;
int run_local_multiplayer(const gbarecomp::GbaNetplayLaunch&,const gbarecomp::GbaNetplayBoot&);
}
