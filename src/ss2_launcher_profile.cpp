#include "ss2_launcher_profile.h"
#include "recomp_launcher.h"
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
namespace ss2 {
bool launcher_recovery_selected = false;
int prepare_recovery_launch(int players, int seat, uint32_t session, int port,
    const char* host, RecompLauncherCNetplayLaunch* out, char* error, size_t cap) {
    auto fail = [&](const char* why) { if (error && cap) std::snprintf(error, cap, "%s", why); return 0; };
    if (!out || players < 2 || players > 4 || seat < 0 || seat >= players ||
        !session || session > 2147483647u || port < 1024 || port > 65535)
        return fail("Revisa jugadores, asiento, sala y puerto.");
    std::string address = host ? host : "";
    if (seat && (address.empty() || address.size() > 55))
        return fail("Introduce la IP de Hamachi del anfitrion o su nombre DNS.");
    if (seat)
        for (unsigned char c : address)
            if (!std::isalnum(c) && c != '.' && c != '-')
                return fail("La direccion debe ser IPv4 o un nombre DNS, sin puerto.");
    *out = {};
    out->enabled = 1;
    out->local_slot = seat;
    out->input_player = -1; // Dashboard controller for this PC.
    out->max_slots = out->player_count = players;
    out->occupied_mask = (1u << players) - 1;
    out->session_id = session;
    out->input_delay = 6;
    out->input_prediction = 6;
    out->slot_port_valid = 1;
    for (int i = 0; i < players; ++i) out->slot_port[i] = i;
    std::snprintf(out->bind_hostport, sizeof(out->bind_hostport), "0.0.0.0:%d", port);
    if (seat) std::snprintf(out->peer_hostport, sizeof(out->peer_hostport), "%s:%d", address.c_str(), port);
    launcher_recovery_selected = true;
    if (error && cap) error[0] = 0;
    return 1;
}
void configure_launcher_profile(RecompLauncherCGameInfo& info) {
    info.experimental_netplay_label = "Netplay (experimental)";
    info.experimental_netplay_prepare = &prepare_recovery_launch;
}
}
