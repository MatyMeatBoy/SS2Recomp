#include "online/ss2_link_protocol.h"
#include "ss2_prediction.h"
#include <cstring>
#include <iostream>

using namespace ss2;
void require(bool condition, const char* error) {
    if (!condition) throw std::runtime_error(error);
}
struct World {
    std::uint32_t tick=0;
    std::array<std::uint32_t,4> positions{};
    RecoveryBytes capture() const {
        RecoveryBytes bytes(sizeof(*this)); std::memcpy(bytes.data(),this,sizeof(*this)); return bytes;
    }
    void restore(std::span<const std::uint8_t> bytes) {
        require(bytes.size()==sizeof(*this),"invalid world snapshot");
        std::memcpy(this,bytes.data(),sizeof(*this));
    }
    void advance(std::span<const std::uint16_t> inputs) {
        for (unsigned i=0;i<inputs.size();++i) positions[i]=positions[i]*1664525u+inputs[i]+1013904223u;
        ++tick;
    }
};
std::uint16_t input(unsigned tick,unsigned seat) { return (tick*13+seat*23)%1024; }
int main() {
    try {
        using namespace ss2::online;
        LinkBlock block{};
        for (unsigned i=0;i<24;++i) block[i]=i*7;
        for (unsigned counter=0;counter<256;++counter) {
            auto packet=encode_driver(block,counter,counter&15);
            require(decode_driver(packet)==block,"driver roundtrip");
            packet[12]^=1;
            require(!decode_driver(packet),"corrupt driver accepted");
        }
        require(!decode_driver(std::span<const std::uint8_t>{}),"truncated packet accepted");
        for (unsigned peer=0;peer<4;++peer) {
            MailboxAssembler assembler(peer);
            LinkBlock announce{}; announce[8]=0xb1; announce[9]=0x91; announce[10]=2;
            require(!assembler.observe(announce),"partial announce emitted");
            LinkBlock chunk{}; chunk[8]=peer; chunk[9]=0xb2; chunk[10]=2;
            std::fill(chunk.begin()+12,chunk.begin()+20,0x22);
            require(!assembler.observe(chunk),"partial payload emitted");
            require(!assembler.observe(chunk),"duplicate emitted");
            chunk[10]=1; std::fill(chunk.begin()+12,chunk.begin()+20,0x11);
            auto message=assembler.observe(chunk);
            require(message && message->payload.size()==16 && message->payload[0]==0x11 &&
                message->payload[8]==0x22 && message->peer==peer,"message reassembly");
            require(!assembler.observe(chunk),"completed message emitted twice");
            assembler.reset(); require(!assembler.observe(chunk),"old reconnect chunk accepted");
            announce[10]=17; assembler.observe(announce);
            require(!assembler.observe(chunk),"oversize message accepted");
        }
        std::uint64_t corrections=0;
        for (unsigned players=2;players<=4;++players)
            for (unsigned seat=0;seat<players;++seat) {
                World predicted,authority;
                RecoveryCallbacks machine;
                machine.capture=[&]{return predicted.capture();};
                machine.restore=[&](auto bytes){predicted.restore(bytes);};
                machine.advance=[&](auto row){predicted.advance(row);};
                PredictionTimeline timeline(players,seat,machine);
                for (unsigned t=0;t<180;++t) {
                    while (timeline.tip()<t+8) timeline.predict(input(timeline.tip(),seat));
                    std::array<std::uint16_t,4> row{};
                    for (unsigned s=0;s<players;++s) row[s]=input(t,s);
                    authority.advance({row.data(),players});
                    timeline.confirm({row.data(),players});
                    while (timeline.replay_pending()) timeline.replay_step();
                    require(timeline.capture()==authority.capture(),"rollback diverged from authority");
                }
                corrections+=timeline.corrections;
                while (timeline.can_predict()) timeline.predict(input(timeline.tip(),seat));
                require(!timeline.can_predict(),"unbounded disconnect prediction");
                timeline.restore(authority.capture()); timeline.synchronize(authority.tick);
                require(predicted.capture()==authority.capture(),"rejoin restore failed");
                for (unsigned t=180;t<240;++t) {
                    timeline.predict(input(t,seat));
                    std::array<std::uint16_t,4> row{};
                    for (unsigned s=0;s<players;++s) row[s]=input(t,s);
                    authority.advance({row.data(),players}); timeline.confirm({row.data(),players});
                    require(timeline.capture()==authority.capture(),"post-rejoin divergence");
                }
            }
        require(corrections>0,"rollback was not exercised");
        std::cout<<"PASS: driver checksum, bounded message assembly, 2-4 seats, delayed inputs, "
            <<corrections<<" rollback corrections, disconnect horizon, authoritative rejoin\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
