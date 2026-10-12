#include "ss2_recovery_runner.h"
#include "gba_bios.h"
#include "host_window.h"
#include "netplay_status_overlay.h"
#include "multiplayer_pacing.h"
#include "multiplayer_session.h"
#include "save_config.h"
#include "ss2_recovery.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <windows.h>

namespace ss2 {
namespace {
void configure_save(gba::GbaSave &save, const gbarecomp::SaveConfiguration &config) {
    if (!gbarecomp::valid_save_size(config.type, config.size))
        throw std::runtime_error("invalid recovery save hardware");
    switch (config.type) {
    case gba::SaveType::SRAM:
        save.configure_sram(config.size);
        break;
    case gba::SaveType::EEPROM:
        save.configure_eeprom(config.size);
        break;
    case gba::SaveType::Flash512:
    case gba::SaveType::Flash1M:
        save.configure_flash(config.size);
        break;
    default:
        throw std::runtime_error("unsupported recovery save hardware");
    }
}
RecoveryBytes read(const std::filesystem::path &path, std::size_t max) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
        throw std::runtime_error("cannot read " + path.string());
    auto n = in.tellg();
    if (n < 0 || static_cast<std::uint64_t>(n) > max)
        throw std::runtime_error("invalid local input size");
    RecoveryBytes b(static_cast<std::size_t>(n));
    in.seekg(0);
    if (!in.read(reinterpret_cast<char *>(b.data()), b.size()))
        throw std::runtime_error("short local input read");
    return b;
}
void load_save(gba::GbaSave &save, std::span<const std::uint8_t> bytes) {
    bool ok = save.flash_enabled() ? save.load_flash_bytes(bytes.data(), bytes.size())
              : save.sram_enabled()
                  ? save.load_sram_bytes(bytes.data(), bytes.size())
                  : save.eeprom_enabled() && save.load_eeprom_bytes(bytes.data(), bytes.size());
    if (!ok)
        throw std::runtime_error("recovery owner save size/type mismatch");
    save.clear_dirty();
}
RecoveryBytes save_bytes(const gba::GbaSave &save) {
    return save.flash_enabled()  ? save.flash_bytes()
           : save.sram_enabled() ? save.sram_bytes()
                                 : save.eeprom_bytes();
}
} // namespace
int run_recoverable_netplay(const gbarecomp::GbaNetplayLaunch &launch,
                            const gbarecomp::GbaNetplayBoot &boot) {
    using namespace gbarecomp;
    validate_gba_netplay_launch(launch);
    if (launch.medium != GbaLinkMedium::Cable || launch.force_input_relay ||
        launch.bind_endpoint.empty() || (launch.local_seat && launch.peer_endpoint.empty()))
        throw std::runtime_error(
            "recoverable netplay requires direct-IP cable mode; use ordinary UDP for lobby/relay");
    if (!launch.resume_path.empty())
        throw std::runtime_error(
            "recoverable rooms resume automatically; use a new session ID for a fresh room");
    if (!boot.bios || !boot.rom)
        throw std::runtime_error("missing verified recovery assets");
    GbaSessionConfig config;
    config.links = {{GbaLinkMedium::Cable, {}}};
    for (unsigned i = 0; i < launch.player_count; ++i) {
        config.machines.push_back({i, launch.program_id, boot.expected_rom_sha1});
        config.links[0].machines.push_back(i);
        config.input_machines.push_back(launch.seat_machine[i]);
    }
    GbaMultiplayerSession simulation(std::move(config));
    simulation.set_native_slices(true);
    for (unsigned i = 0; i < launch.player_count; ++i) {
        auto &m = simulation.machine(i);
        m.bus.set_bios(boot.bios);
        m.bus.set_rom(boot.rom->data(), boot.rom->size());
        configure_save(m.bus.save(), boot.save);
        m.ppu.set_rasterization_enabled(false);
        if (launch.setup_instance)
            launch.setup_instance(m);
    }
    auto &local_save = simulation.input_machine(launch.local_seat).bus.save();
    if (std::filesystem::exists(boot.local_save))
        load_save(local_save, read(boot.local_save, 128 * 1024));
    const bool testing = std::getenv("SS2_RECOVERY_TEST") && boot.finish_tick;
    if (testing)
        if (const char *path = std::getenv("SS2_RECOVERY_INITIAL_STATE")) {
            auto bytes = read(path, 8 * 1024 * 1024);
            std::string error;
            if (!simulation.load_state(bytes, &error))
                throw std::runtime_error(error);
        }
    std::vector<std::pair<std::uint32_t, std::array<std::uint16_t, 4>>> script;
    if (testing)
        if (const char *path = std::getenv("SS2_RECOVERY_INPUTS")) {
            std::ifstream in(path);
            if (!in)
                throw std::runtime_error("cannot read recovery input script");
            std::string line;
            while (std::getline(in, line)) {
                if (line.empty() || line[0] == '#')
                    continue;
                std::istringstream row(line);
                std::uint32_t tick;
                std::array<std::uint16_t, 4> buttons{};
                if (!(row >> tick))
                    throw std::runtime_error("invalid recovery script tick");
                for (unsigned i = 0; i < launch.player_count; ++i) {
                    unsigned key;
                    if (!(row >> std::setbase(0) >> key) || key > 1023)
                        throw std::runtime_error("invalid recovery script keys");
                    buttons[i] = static_cast<std::uint16_t>(key);
                }
                if (!script.empty() && tick <= script.back().first)
                    throw std::runtime_error("unordered recovery script");
                script.emplace_back(tick, buttons);
            }
        }
    const unsigned width = gba_netplay_view_width(launch.view, 0, 0, launch.view_policy.max_width);
    GbaNetplayPresentation presentation(launch.view_policy, boot.affine_filter);
    presentation.request_width(width);
    auto attach = [&] {
        simulation.input_machine(launch.local_seat).ppu.set_presentation_observer(&presentation);
    };
    attach();
    struct Detach {
        GbaMultiplayerSession &s;
        unsigned seat;
        ~Detach() { s.input_machine(seat).ppu.set_presentation_observer(nullptr); }
    } detach{simulation, launch.local_seat};
    const bool cold = simulation.cycle() == 0;
    RecoveryCallbacks callbacks;
    callbacks.capture = [&] { return simulation.save_state(); };
    callbacks.restore = [&](auto bytes) {
        std::string error;
        if (!simulation.load_state(bytes, &error))
            throw std::runtime_error("recovery restore: " + error);
        simulation.discard_audio_output();
        attach();
    };
    std::uint32_t qualification_frame = 0;
    bool corrupted = false;
    callbacks.advance = [&](auto buttons) {
        simulation.discard_audio_output();
        simulation.run_frame(buttons);
        ++qualification_frame;
        if (testing && !corrupted)
            if (const char *at = std::getenv("SS2_RECOVERY_CORRUPT_AT"))
                if (qualification_frame == std::stoul(at)) {
                    simulation.input_machine(launch.local_seat).bus.ewram_ptr()[256 * 1024 - 1] ^=
                        1;
                    corrupted = true;
                    std::printf("recovery corruption injected=%u\n", qualification_frame);
                    std::fflush(stdout);
                }
    };
    if (!script.empty())
        callbacks.sample_input = [&](std::uint32_t tick) {
            std::array<std::uint16_t, 4> row{};
            for (auto &entry : script) {
                if (entry.first > tick)
                    break;
                row = entry.second;
            }
            return row[launch.local_seat];
        };
    callbacks.owner_save = save_bytes(simulation.input_machine(launch.local_seat).bus.save());
    callbacks.install_owner = [&](unsigned seat, auto bytes) {
        if (cold)
            load_save(simulation.input_machine(seat).bus.save(), bytes);
    };
    RecoveryConfig room_config;
    room_config.players = launch.player_count;
    room_config.seat = launch.local_seat;
    room_config.session = launch.session_id;
    room_config.finish_tick = boot.finish_tick;
    room_config.bind = launch.bind_endpoint;
    room_config.peer = launch.peer_endpoint;
    room_config.identity = "ss2-recovery/1:" + launch.program_id + ":" + launch.build_identity +
                           ":" + boot.expected_rom_sha1 + ":" + boot.bios->sha1_hex() + ":" +
                           std::to_string(static_cast<unsigned>(boot.save.type)) + ":" +
                           std::to_string(boot.save.size) + ":" +
                           gba_netplay_seat_identity(launch) + ":" +
                           std::to_string(boot.finish_tick);
    room_config.directory = boot.config_directory / "netplay" / "recovery" /
                            ("room-" + std::to_string(launch.session_id)) /
                            ("seat-" + std::to_string(launch.local_seat));
    if (testing)
        if (const char *path = std::getenv("SS2_RECOVERY_DIRECTORY"))
            room_config.directory = path;
    RecoveryRoom room(std::move(room_config), std::move(callbacks));
    HostWindow window;
    if (!window.open(boot.scale, width, 160, boot.title.c_str(), boot.screen.c_str(),
                     boot.linear_filter, boot.sharp_filter, launch.view == GbaNetplayView::Adaptive,
                     true))
        throw std::runtime_error("cannot open recovery window");
    window.load_input_config(boot.config_directory.string().c_str(), false);
    window.set_fullscreen(boot.fullscreen);
    window.set_volume(boot.volume);
    window.set_host_overlay(&draw_netplay_wait);
    std::vector<std::uint8_t> waiting_image(width*160*3,17);
    GbaNetplayPacer pacer;
    std::string old_title;
    std::uint32_t reported = UINT32_MAX;
    while (!room.finished()) {
        auto events = window.pump();
        if (events.quit || events.terminating)
            return 0;
        if (events.toggle_fullscreen)
            window.set_fullscreen(window.fullscreen() ? 0 : 1);
        if (events.window_bigger)
            window.adjust_scale(1);
        if (events.window_smaller)
            window.adjust_scale(-1);
        if (events.volume_up)
            window.set_volume(window.volume() + 5);
        if (events.volume_down)
            window.set_volume(window.volume() - 5);
        if (launch.view == GbaNetplayView::Adaptive) {
            int w = 0, h = 0;
            if (window.drawable_size(&w, &h))
                presentation.request_width(
                    gba_netplay_view_width(launch.view, w, h, launch.view_policy.max_width));
        }
        std::uint16_t buttons = static_cast<std::uint16_t>(~events.keyinput & 1023);
        if (!script.empty()) {
            std::array<std::uint16_t, 4> row{};
            for (auto &entry : script) {
                if (entry.first > room.tick())
                    break;
                row = entry.second;
            }
            buttons = row[launch.local_seat];
        }
        auto now = GbaNetplayPacer::Clock::now();
        bool advanced = room.poll(buttons, pacer.ready(now));
        if (advanced) {
            pacer.forwarded(now);
            if (auto *pixels = presentation.pixels()) {
                auto w = presentation.width();
                if (!window.set_surface_size(w, 160))
                    throw std::runtime_error("cannot resize recovery surface");
                window.set_view_margins((w - 240) / 2, (w - 240) - (w - 240) / 2, 0, 0);
                window.present(pixels);
            }
            std::array<std::int16_t, 2048> samples{};
            auto &audio = simulation.input_machine(launch.local_seat).bus.audio();
            while (auto count = audio.drain_samples(samples.data(), samples.size()))
                window.push_audio_samples(samples.data(), count);
            simulation.discard_audio_output();
        }
        auto status=room.status();
        netplay_wait_message=status.rfind("Online",0)==0 ? "" : status;
        if (!netplay_wait_message.empty()) {
            if (!window.set_surface_size(width,160))
                throw std::runtime_error("cannot resize recovery waiting-room surface");
            window.present(waiting_image.data());
        }
        auto title = boot.title + " - " + status;
        if (title != old_title) {
            window.set_title(title.c_str());
            old_title = title;
        }
        if (testing && room.tick() != reported && room.tick() % 30 == 0) {
            reported = room.tick();
            std::printf("recovery committed=%u seat=%u\n", reported, launch.local_seat);
            std::fflush(stdout);
        }
        // Qualification-only crash: a durable marker makes the watchdog test
        // terminate once rather than entering a crash loop. Never active in
        // an ordinary unbounded game session.
        if (testing)
            if (const char *at = std::getenv("SS2_RECOVERY_TEST_CRASH_AT"))
                if (room.tick() >= std::stoul(at))
                    if (const char *directory = std::getenv("SS2_RECOVERY_DIRECTORY")) {
                        auto marker = std::filesystem::path(directory) / "qualification.crashed";
                        if (!std::filesystem::exists(marker)) {
                            std::ofstream file(marker);
                            file << room.tick();
                            file.close();
                            TerminateProcess(GetCurrentProcess(), 0xc0000005u);
                        }
                    }
        if (!advanced)
            pacer.idle();
    }
    if (testing)
        if (const char *path = std::getenv("SS2_RECOVERY_STATE_OUT")) {
            auto b = simulation.save_state();
            std::ofstream f(path, std::ios::binary);
            if (!f.write(reinterpret_cast<char *>(b.data()), b.size()))
                throw std::runtime_error("cannot write qualification state");
        }
    return 0;
}
} // namespace ss2
