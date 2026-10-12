#include "ss2_link_protocol.h"
#include <algorithm>
#include <stdexcept>

namespace ss2::online {
namespace {
std::uint16_t word(std::span<const std::uint8_t> b, unsigned i) {
    return b[i] | (std::uint16_t(b[i+1]) << 8);
}
}
DriverPacket encode_driver(const LinkBlock& block, std::uint8_t counter, std::uint8_t errors) {
    if (errors > 15) throw std::invalid_argument("invalid MultiSio error mask");
    DriverPacket packet{};
    packet[0] = counter; packet[1] = errors;
    std::copy(block.begin(), block.end(), packet.begin()+4);
    std::uint16_t sum = 0;
    for (unsigned i=0; i<28; i+=2) sum += word(packet,i);
    const auto checksum = static_cast<std::uint16_t>(~sum - 0x10);
    packet[2] = checksum; packet[3] = checksum >> 8;
    return packet;
}
std::optional<LinkBlock> decode_driver(std::span<const std::uint8_t> packet) {
    if (packet.size()!=32 || (packet[1]&0xf0)) return {};
    std::uint16_t sum=0;
    for (unsigned i=0; i<28; i+=2) sum += word(packet,i);
    if (sum!=0xffef) return {};
    LinkBlock block;
    std::copy_n(packet.begin()+4,24,block.begin());
    return block;
}
MailboxAssembler::MailboxAssembler(unsigned peer) : peer_(peer) {
    if (peer>=4) throw std::invalid_argument("invalid link peer");
}
void MailboxAssembler::reset() {
    tag_=chunks_=0; received_=0; emitted_=false; payload_.fill(0);
}
std::optional<LinkMessage> MailboxAssembler::observe(const LinkBlock& block) {
    if (block[8]==0xb1 && block[9]>=0x81 && block[9]<=0xb0) {
        if (!block[10] || block[10]>16) { reset(); return {}; }
        if (tag_!=block[9] || chunks_!=block[10] || emitted_) {
            reset(); tag_=block[9]; chunks_=block[10];
        }
        return {};
    }
    if (!tag_ || emitted_ || block[8]!=peer_ || block[9]!=0xb2 ||
        !block[10] || block[10]>chunks_) return {};
    const unsigned index=block[10]-1;
    // A conflicting duplicate is a protocol error, not another gameplay event.
    if (received_ & (1u<<index)) {
        if (!std::equal(block.begin()+12,block.begin()+20,payload_.begin()+index*8)) {
            reset(); throw std::runtime_error("conflicting link message chunk");
        }
        return {};
    }
    std::copy_n(block.begin()+12,8,payload_.begin()+index*8);
    received_ |= 1u<<index;
    if (received_!=((1u<<chunks_)-1)) return {};
    emitted_=true;
    return LinkMessage{static_cast<std::uint8_t>(peer_),tag_,
        {payload_.begin(),payload_.begin()+chunks_*8}};
}
} // namespace ss2::online
