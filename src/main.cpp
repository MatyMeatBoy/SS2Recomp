#include <cstdio>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

#include "runtime.h"
#include "ss2_extended_view.h"
#include "ss2_flash_dispatch.h"
#if defined(GBAGAME_NETPLAY)
#include "multiplayer_launch.h"
#include "ss2_netplay_policy.h"
#if defined(_WIN32)
#include "ss2_recovery_runner.h"
#endif
#include "multiplayer_session.h"
#include "gba_netplay_build_identity.h"
#endif

#if defined(GBAGAME_RECOMP_UI)
#include "game_launcher_boot.h"
#endif

namespace {

#if defined(GBAGAME_NETPLAY)
int (*ordinary_netplay_run)(const gbarecomp::GbaNetplayLaunch&,const gbarecomp::GbaNetplayBoot&)=nullptr;
int safe_netplay_run(const gbarecomp::GbaNetplayLaunch& requested, const gbarecomp::GbaNetplayBoot& boot) {
    auto launch=requested;
    ss2::apply_safe_netplay_policy(launch);
#if defined(_WIN32)
    if (!launch.force_input_relay && !launch.bind_endpoint.empty())
        return ss2::run_recoverable_netplay(launch,boot);
#endif
    return ordinary_netplay_run(launch,boot);
}

void setup_netplay_instance(gbarecomp::GbaInstance& instance) {
    // Each console restores its own execution context at every scheduler turn.
    // The single-player global hook alone does not survive that context switch.
    instance.execution.ram_dispatch = &ss2::flash_stack_dispatch;
}
#endif

void print_usage() {
    std::printf(
        "ShiningSoul2Recomp [--bios <path>] [--rom <path>] [game.toml]\n"
        "  Direct-IP netplay (2-4 consoles, seat 0 hosts):\n"
        "    --netplay-players 4 --netplay-seat N --netplay-session ID\n"
        "    --netplay-bind IP:PORT [--netplay-peer HOST_IP:PORT]\n"
        "    [--netplay-rollback | --netplay-delay-sync] [--netplay-delay 2..20]\n"
        "    --netplay-safe: direct TCP, confirmed inputs, automatic room recovery\n");
}

}  // namespace

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 ||
            std::strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        }
    }

    g_runtime_ram_dispatch_hook = &ss2::flash_stack_dispatch;
    gbarecomp::RunOptions opts;
    std::vector<std::string> args(argv, argv + argc);
#if defined(GBAGAME_NETPLAY)
    bool safe_profile=false;
    for (auto it=args.begin()+1; it!=args.end();) {
        if (*it=="--netplay-safe") { safe_profile=true; it=args.erase(it); }
        else ++it;
    }
    // Content identity: SHA-256 of the supported Europe ROM.
    opts.netplay = gbarecomp::make_gba_netplay_launch("shining-soul-2-eu",
        GBARECOMP_NETPLAY_BUILD_ID,
        "e5c64a1740263e9b26120e487f3688fbbf7b3954a2c043c0d84c333df467e0b2",
        &setup_netplay_instance);
    // Shining Soul II supports four players on one multiplayer cable.
    opts.netplay->max_players = 4;
    opts.netplay->player_count = 4;
    opts.netplay->view_policy = ss2::netplay_view_policy();
    if (safe_profile) {
        ordinary_netplay_run=opts.netplay->run;
        opts.netplay->run=&safe_netplay_run;
    }
    try { gbarecomp::parse_gba_netplay_arguments(args, *opts.netplay); }
    catch (const std::exception& e) {
        std::fprintf(stderr, "netplay: %s\n", e.what());
        return 1;
    }
#endif
    opts.builtin_game_name = "Shining Soul II (Europe)";
    opts.builtin_rom_sha1 = "aa1288be9257337abdfe3034898cf6c7aa778fbc";
    opts.builtin_rom_crc32 = 0x55E503C1u;
    opts.mod_game_id = "shining-soul-2-eu";
    ss2::configure_run_options(opts);
    opts.launcher_region = "Europe";
    // The launcher reads [rom].path and [bios].path from this file when its
    // per-user cache is empty, so the verified local ROM is preselected.
    opts.launcher_game_config = "game.toml";
    opts.launcher_save_path = "saves/shining_soul_2_europe.sav";

#if defined(GBAGAME_RECOMP_UI)
    if (game_launcher_preboot(args, opts)) return 0;
#endif
    std::vector<char*> av;
    av.reserve(args.size());
    for (auto& arg : args) av.push_back(arg.data());
    return gbarecomp::run_game(static_cast<int>(av.size()), av.data(), opts);
}
