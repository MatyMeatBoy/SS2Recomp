#pragma once
#include "ss2_recovery.h"
#include <algorithm>
#include <deque>
#include <map>
#include <stdexcept>

namespace ss2 {
// One linked world, not a second simulation. Only confirmed snapshots may be
// hashed, saved or sent to another peer. Presentation may run ahead of it.
class PredictionTimeline {
    struct Frame {
        std::uint32_t tick;
        std::array<std::uint16_t, 4> inputs;
        RecoveryBytes after;
    };
    RecoveryCallbacks machine_;
    unsigned players_, seat_;
    std::uint32_t confirmed_ = 0, tip_ = 0, target_ = 0;
    RecoveryBytes canonical_;
    std::array<std::uint16_t, 4> held_{};
    std::deque<Frame> frames_;
    std::map<std::uint32_t, std::uint16_t> local_;
public:
    static constexpr unsigned horizon = 24;
    std::uint64_t forwards = 0, replays = 0, corrections = 0;
    PredictionTimeline(unsigned players, unsigned seat, RecoveryCallbacks machine)
        : machine_(std::move(machine)), players_(players), seat_(seat),
          canonical_(machine_.capture()) {
        if (players<2 || players>4 || seat>=players)
            throw std::invalid_argument("invalid prediction topology");
    }
    RecoveryBytes capture() const { return canonical_; }
    void restore(std::span<const std::uint8_t> state) {
        machine_.restore(state);
        canonical_.assign(state.begin(), state.end());
        frames_.clear(); local_.clear(); held_ = {};
        tip_ = target_ = confirmed_;
    }
    void synchronize(std::uint32_t tick) { confirmed_ = tip_ = target_ = tick; }
    std::uint32_t tip() const { return tip_; }
    bool can_predict() const { return tip_ == target_ && tip_ - confirmed_ < horizon; }
    bool replay_pending() const { return tip_ < target_; }
    std::size_t history_bytes() const {
        std::size_t bytes = canonical_.size();
        for (auto &frame : frames_) bytes += frame.after.size();
        return bytes;
    }
    void confirm(std::span<const std::uint16_t> inputs) {
        if (inputs.size()!=players_ || std::any_of(inputs.begin(),inputs.end(),[](auto b){return b>1023;}))
            throw std::invalid_argument("invalid confirmed input frame");
        std::array<std::uint16_t, 4> row{};
        std::copy(inputs.begin(), inputs.end(), row.begin());
        if (!frames_.empty() && frames_.front().tick == confirmed_ &&
            frames_.front().inputs == row) {
            canonical_ = std::move(frames_.front().after);
            frames_.pop_front();
        } else {
            if (tip_ > confirmed_) ++corrections;
            machine_.restore(canonical_);
            machine_.advance(inputs);
            canonical_ = machine_.capture();
            frames_.clear();
            tip_ = confirmed_ + 1;
        }
        local_.erase(confirmed_);
        ++confirmed_;
        target_ = std::max(target_, confirmed_);
        held_ = row;
    }
    void step(std::uint16_t buttons) {
        auto row = held_;
        row[seat_] = buttons;
        machine_.advance({row.data(), players_});
        frames_.push_back({tip_, row, machine_.capture()});
        ++tip_;
    }
    void predict(std::uint16_t buttons) {
        if (buttons>1023) throw std::invalid_argument("invalid predicted input");
        if (!can_predict()) throw std::runtime_error("prediction horizon exhausted");
        local_[tip_] = buttons;
        step(buttons);
        target_ = tip_;
        ++forwards;
    }
    bool replay_step() {
        if (!replay_pending()) return false;
        auto it = local_.find(tip_);
        if (it == local_.end()) throw std::runtime_error("missing rollback local input");
        step(it->second);
        ++replays;
        return true;
    }
};
} // namespace ss2
