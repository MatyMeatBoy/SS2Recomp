#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ss2::online {
// EU AU2P contract, verified against SS2Decomp 70497b6:
// sub_0800191C / sub_08001990. The legacy multi_sio.h says 20 bytes;
// matched functions and structs.h actually copy 24. Never use host structs
// as a wire layout or transmit the driver's four-byte overrun catcher.
using LinkBlock = std::array<std::uint8_t, 24>;
using DriverPacket = std::array<std::uint8_t, 32>;
DriverPacket encode_driver(const LinkBlock&, std::uint8_t counter, std::uint8_t errors);
std::optional<LinkBlock> decode_driver(std::span<const std::uint8_t>);
constexpr std::uint32_t send_address = 0x03002710;
constexpr std::uint32_t receive_address = 0x03002770;

struct LinkMessage {
    std::uint8_t peer, tag;
    std::vector<std::uint8_t> payload;
};
// Extracts the application's announce/chunk mailbox into a complete reliable
// event. It never fabricates a completed message from predicted partial data.
// Modern transport must supply its own epoch and event sequence/deduplication.
class MailboxAssembler {
    unsigned peer_;
    std::uint8_t tag_ = 0, chunks_ = 0;
    std::uint16_t received_ = 0;
    bool emitted_ = false;
    std::array<std::uint8_t, 128> payload_{};
public:
    explicit MailboxAssembler(unsigned peer);
    void reset();
    std::optional<LinkMessage> observe(const LinkBlock&);
};
} // namespace ss2::online
