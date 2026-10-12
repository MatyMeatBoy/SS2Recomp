#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace ss2 {
using RecoveryBytes = std::vector<std::uint8_t>;
struct RecoveryConfig {
    unsigned players = 2, seat = 0;
    std::uint32_t session = 0, finish_tick = 0;
    std::string bind, peer, identity;
    std::filesystem::path directory;
    bool streaming = false;
    bool authoritative = false;
};
// Guest-independent seam. Capture includes the entire deterministic system.
// Restore must be atomic; render/audio state is never an authority for recovery.
struct RecoveryCallbacks {
    std::function<RecoveryBytes()> capture;
    std::function<void(std::span<const std::uint8_t>)> restore;
    std::function<void(std::span<const std::uint16_t>)> advance;
    // Optional deterministic test/replay source, sampled at the requested tick.
    std::function<std::uint16_t(std::uint32_t)> sample_input;
    RecoveryBytes owner_save;
    std::function<void(unsigned, std::span<const std::uint8_t>)> install_owner;
    std::function<void(std::uint32_t)> synchronized;
};
class RecoveryRoom {
  public:
    RecoveryRoom(RecoveryConfig, RecoveryCallbacks);
    ~RecoveryRoom();
    RecoveryRoom(const RecoveryRoom &) = delete;
    RecoveryRoom &operator=(const RecoveryRoom &) = delete;
    // Nonblocking I/O. True means one frame became committed, not predicted.
    bool poll(std::uint16_t buttons, bool allow_frame);
    std::uint32_t tick() const;
    bool ready() const;
    void submit_input(std::uint32_t tick, std::uint16_t buttons);
    bool finished() const;
    std::string status() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace ss2
