#include "ss2_recovery.h"
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>

int main(int argc, char **argv) {
    try {
        if (argc != 7)
            throw std::runtime_error("seat players port ticks directory corruption-tick");
        unsigned seat = std::stoul(argv[1]), players = std::stoul(argv[2]),
                 port = std::stoul(argv[3]), ticks = std::stoul(argv[4]),
                 corruption = std::stoul(argv[6]);
        struct State {
            std::uint32_t tick = 0, value = 17;
            std::array<std::uint32_t, 4> seats{11, 19, 29, 37};
        } state;
        static_assert(sizeof(State) == 24);
        bool corrupted = false;
        ss2::RecoveryCallbacks callbacks;
        callbacks.capture = [&] {
            ss2::RecoveryBytes b(sizeof(state));
            std::memcpy(b.data(), &state, sizeof(state));
            return b;
        };
        callbacks.restore = [&](auto bytes) {
            if (bytes.size() != sizeof(state))
                throw std::runtime_error("bad toy state");
            std::memcpy(&state, bytes.data(), sizeof(state));
        };
        callbacks.advance = [&](auto inputs) {
            for (unsigned i = 0; i < players; ++i) {
                state.seats[i] = state.seats[i] * 1664525u + inputs[i] + 1013904223u;
                state.value ^= state.seats[i];
            }
            ++state.tick;
            if (corruption && state.tick == corruption && !corrupted) {
                state.value ^= 0x1234;
                corrupted = true;
            }
        };
        callbacks.sample_input = [&](std::uint32_t tick) {
            return static_cast<std::uint16_t>((tick * 13 + seat * 7) % 1024);
        };
        ss2::RecoveryConfig config;
        config.players = players;
        config.seat = seat;
        config.session = 72111;
        config.finish_tick = ticks;
        config.identity = "ss2-protocol-test-v1";
        config.directory = argv[5];
        config.bind = "127.0.0.1:" + std::to_string(port);
        config.peer = config.bind;
        ss2::RecoveryRoom room(config, std::move(callbacks));
        unsigned previous = UINT32_MAX;
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(180);
        while (!room.finished()) {
            room.poll(static_cast<std::uint16_t>((room.tick() * 13 + seat * 7) % 1024), true);
            if (room.tick() != previous && room.tick() % 10 == 0) {
                previous = room.tick();
                std::cout << "committed=" << room.tick() << std::endl;
            }
            if (std::chrono::steady_clock::now() > deadline)
                throw std::runtime_error("test deadline");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        std::ofstream f(config.directory / "final.state", std::ios::binary);
        f.write(reinterpret_cast<char *>(&state), sizeof(state));
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
