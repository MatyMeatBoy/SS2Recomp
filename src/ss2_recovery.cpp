#include "ss2_recovery.h"
#include "sha1.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <io.h>
#include <map>
#include <stdexcept>
#include <utility>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
// clang-format off
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
// clang-format on

namespace ss2 {
namespace {
using Clock = std::chrono::steady_clock;
using Digest = std::array<std::uint8_t, 20>;
constexpr std::uint32_t magic = 0x32525353, version = 1;
constexpr std::size_t max_body = 8 * 1024 * 1024, max_queue = 2 * max_body;
enum Type : std::uint32_t {
    Hello = 1,
    State,
    Ready,
    AskInput,
    Input,
    Advance,
    Ack,
    Commit,
    Ping,
    Pong,
    End,
    EndAck,
    Rejected,
    StreamInput,
    StreamFrame,
    StreamHash
};
void put(RecoveryBytes &b, std::uint32_t x) {
    for (unsigned i = 0; i < 4; ++i)
        b.push_back(static_cast<std::uint8_t>(x >> (i * 8)));
}
struct Reader {
    std::span<const std::uint8_t> b;
    std::uint32_t word() {
        if (b.size() < 4)
            throw std::runtime_error("truncated recovery message");
        std::uint32_t x = 0;
        for (unsigned i = 0; i < 4; ++i)
            x |= std::uint32_t(b[i]) << (8 * i);
        b = b.subspan(4);
        return x;
    }
    RecoveryBytes blob(std::size_t limit = max_body) {
        auto n = word();
        if (n > limit || n > b.size())
            throw std::runtime_error("invalid recovery payload size");
        RecoveryBytes out(b.begin(), b.begin() + n);
        b = b.subspan(n);
        return out;
    }
    std::string text(std::size_t limit = 2048) {
        auto v = blob(limit);
        return {v.begin(), v.end()};
    }
    Digest digest() {
        if (b.size() < 20)
            throw std::runtime_error("missing recovery digest");
        Digest d;
        std::copy_n(b.begin(), 20, d.begin());
        b = b.subspan(20);
        return d;
    }
    void done() {
        if (!b.empty())
            throw std::runtime_error("trailing recovery bytes");
    }
};
void blob(RecoveryBytes &b, std::span<const std::uint8_t> v) {
    put(b, static_cast<std::uint32_t>(v.size()));
    b.insert(b.end(), v.begin(), v.end());
}
void text(RecoveryBytes &b, const std::string &s) {
    blob(b, {reinterpret_cast<const std::uint8_t *>(s.data()), s.size()});
}
Digest digest(std::span<const std::uint8_t> b) { return gba::sha1(b.data(), b.size()).bytes; }
void append_digest(RecoveryBytes &b, const Digest &d) { b.insert(b.end(), d.begin(), d.end()); }
RecoveryBytes read_file(const std::filesystem::path &p, std::size_t limit) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f)
        throw std::runtime_error("cannot read recovery file: " + p.string());
    auto n = f.tellg();
    if (n < 0 || static_cast<std::uint64_t>(n) > limit)
        throw std::runtime_error("invalid recovery file length");
    RecoveryBytes b(static_cast<std::size_t>(n));
    f.seekg(0);
    if (!f.read(reinterpret_cast<char *>(b.data()), b.size()))
        throw std::runtime_error("short recovery file read");
    return b;
}
void durable_write(const std::filesystem::path &p, std::span<const std::uint8_t> b,
                   bool append = false) {
    FILE *f = _wfopen(p.c_str(), append ? L"ab" : L"wb");
    if (!f)
        throw std::runtime_error("cannot open recovery storage");
    bool ok = std::fwrite(b.data(), 1, b.size(), f) == b.size() && std::fflush(f) == 0 &&
              _commit(_fileno(f)) == 0;
    if (std::fclose(f) != 0)
        ok = false;
    if (!ok)
        throw std::runtime_error("cannot durably write recovery storage");
}
void atomic_write(const std::filesystem::path &p, std::span<const std::uint8_t> b) {
    auto tmp = p;
    tmp += L".tmp";
    durable_write(tmp, b);
    if (!MoveFileExW(tmp.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("cannot atomically replace recovery checkpoint");
}
std::string random_token() {
    std::array<unsigned char, 16> b{};
    if (BCryptGenRandom(nullptr, b.data(), static_cast<ULONG>(b.size()),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
        throw std::runtime_error("cannot generate recovery seat credential");
    const char *hex = "0123456789abcdef";
    std::string s;
    for (auto x : b) {
        s += hex[x >> 4];
        s += hex[x & 15];
    }
    return s;
}
struct Sockets {
    Sockets() {
        WSADATA w{};
        if (WSAStartup(MAKEWORD(2, 2), &w))
            throw std::runtime_error("Winsock startup failed");
    }
    ~Sockets() { WSACleanup(); }
};
void nonblocking(SOCKET s) {
    u_long yes = 1;
    if (ioctlsocket(s, FIONBIO, &yes))
        throw std::runtime_error("cannot configure socket");
}
void tune(SOCKET s) {
    int yes = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char *>(&yes), sizeof(yes));
    nonblocking(s);
}
sockaddr_in endpoint(const std::string &value, bool passive) {
    auto colon = value.rfind(':');
    if (colon == std::string::npos)
        throw std::runtime_error("recovery endpoint requires HOST:PORT");
    auto host = value.substr(0, colon), port = value.substr(colon + 1);
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = passive ? AI_PASSIVE : 0;
    addrinfo *list = nullptr;
    if (getaddrinfo(host.empty() ? nullptr : host.c_str(), port.c_str(), &hints, &list) || !list)
        throw std::runtime_error("cannot resolve recovery endpoint");
    auto address = *reinterpret_cast<sockaddr_in *>(list->ai_addr);
    freeaddrinfo(list);
    return address;
}
struct Packet {
    Type type;
    std::uint32_t epoch, tick;
    RecoveryBytes body;
};
struct Connection {
    SOCKET socket = INVALID_SOCKET;
    RecoveryBytes rx, tx;
    std::size_t sent = 0;
    int seat = -1;
    std::uint32_t verified = 0;
    bool connecting = false, ready = false, input = false, ack = false, ended = false;
    Clock::time_point heard = Clock::now(), ping = Clock::now();
    Connection() = default;
    explicit Connection(SOCKET s) : socket(s) {}
    ~Connection() { close(); }
    void close() {
        if (socket != INVALID_SOCKET)
            closesocket(socket);
        socket = INVALID_SOCKET;
        rx.clear();
        tx.clear();
        sent = 0;
        ready = false;
    }
    void queue(Type type, std::uint32_t epoch, std::uint32_t tick, const RecoveryBytes &body = {}) {
        if (socket == INVALID_SOCKET)
            return;
        if (body.size() > max_body || tx.size() - sent + body.size() + 24 > max_queue)
            throw std::runtime_error("recovery send queue limit");
        if (sent) {
            tx.erase(tx.begin(), tx.begin() + sent);
            sent = 0;
        }
        put(tx, magic);
        put(tx, version);
        put(tx, type);
        put(tx, epoch);
        put(tx, tick);
        put(tx, static_cast<std::uint32_t>(body.size()));
        tx.insert(tx.end(), body.begin(), body.end());
    }
    bool pump() {
        if (socket == INVALID_SOCKET)
            return false;
        if (connecting) {
            fd_set writable, errors;
            FD_ZERO(&writable);
            FD_ZERO(&errors);
            FD_SET(socket, &writable);
            FD_SET(socket, &errors);
            timeval zero{};
            if (select(0, nullptr, &writable, &errors, &zero) <= 0)
                return true;
            int error = 0, len = sizeof(error);
            getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&error), &len);
            if (error || FD_ISSET(socket, &errors)) {
                close();
                return false;
            }
            connecting = false;
            heard = Clock::now();
        }
        std::size_t budget = 256 * 1024;
        while (sent < tx.size() && budget) {
            auto count = std::min({tx.size() - sent, budget, std::size_t(65536)});
            int n = send(socket, reinterpret_cast<const char *>(tx.data() + sent),
                         static_cast<int>(count), 0);
            if (n < 0) {
                if (WSAGetLastError() == WSAEWOULDBLOCK)
                    break;
                close();
                return false;
            }
            if (!n) {
                close();
                return false;
            }
            sent += n;
            budget -= n;
        }
        if (sent == tx.size()) {
            tx.clear();
            sent = 0;
        }
        budget = 256 * 1024;
        std::array<std::uint8_t, 65536> block{};
        while (budget) {
            int n = recv(socket, reinterpret_cast<char *>(block.data()),
                         static_cast<int>(std::min(budget, block.size())), 0);
            if (n < 0) {
                if (WSAGetLastError() == WSAEWOULDBLOCK)
                    break;
                close();
                return false;
            }
            if (!n) {
                close();
                return false;
            }
            heard = Clock::now();
            budget -= n;
            rx.insert(rx.end(), block.begin(), block.begin() + n);
            if (rx.size() > max_queue) {
                close();
                return false;
            }
        }
        if (Clock::now() - heard > std::chrono::seconds(8)) {
            close();
            return false;
        }
        return true;
    }
    bool next(Packet &p) {
        if (rx.size() < 24)
            return false;
        Reader r{std::span<const std::uint8_t>(rx).first(24)};
        if (r.word() != magic || r.word() != version)
            throw std::runtime_error("incompatible recovery protocol");
        auto type = r.word();
        p.epoch = r.word();
        p.tick = r.word();
        auto length = r.word();
        if (type < Hello || type > StreamHash || length > max_body)
            throw std::runtime_error("invalid recovery frame");
        if (rx.size() < 24 + length)
            return false;
        p.type = static_cast<Type>(type);
        p.body.assign(rx.begin() + 24, rx.begin() + 24 + length);
        rx.erase(rx.begin(), rx.begin() + 24 + length);
        return true;
    }
    void heartbeat(std::uint32_t epoch, std::uint32_t tick) {
        if (!connecting && Clock::now() - ping >= std::chrono::seconds(1)) {
            queue(Ping, epoch, tick);
            ping = Clock::now();
        }
    }
};
} // namespace

struct RecoveryRoom::Impl {
    Sockets sockets;
    RecoveryConfig config;
    RecoveryCallbacks callbacks;
    SOCKET listener = INVALID_SOCKET;
    HANDLE storage_lock = INVALID_HANDLE_VALUE;
    std::vector<std::unique_ptr<Connection>> links;
    std::array<Connection *, 4> seats{};
    std::array<std::string, 4> credentials{};
    std::array<RecoveryBytes, 4> owners{};
    std::array<std::uint16_t, 4> inputs{};
    std::string credential;
    std::string fatal_error;
    RecoveryBytes committed, candidate;
    Digest committed_hash{}, candidate_hash{};
    std::uint32_t tick = 0, epoch = 1;
    bool initialized = false, awaiting_input = false, awaiting_ack = false, done = false,
         ending = false;
    unsigned repairs = 0;
    Clock::time_point retry = Clock::now(), phase_started = Clock::now();
    std::filesystem::path checkpoint, journal, meta;
    struct InputRow {
        std::array<std::uint16_t, 4> keys{};
        unsigned mask = 0;
    };
    std::map<std::uint32_t, InputRow> future_inputs;
    std::map<std::uint32_t, Digest> recent_hashes;
    void restore_committed() {
        callbacks.restore(committed);
        if (callbacks.synchronized) callbacks.synchronized(tick);
    }
    bool ready() const {
        return initialized && !ending && !done &&
            (config.seat == 0 ? all(&Connection::ready) :
             !links.empty() && links[0]->ready);
    }
    void accept_input(unsigned seat, std::uint32_t frame, std::uint16_t buttons) {
        if (frame < tick) return;
        if (frame - tick >= 128 || buttons > 1023)
            throw std::runtime_error("input outside bounded streaming window");
        auto &row = future_inputs[frame];
        if ((row.mask & (1u << seat)) && row.keys[seat] != buttons)
            throw std::runtime_error("local input changed after submission");
        row.mask |= 1u << seat;
        row.keys[seat] = buttons;
    }
    void submit_input(std::uint32_t frame, std::uint16_t buttons) {
        if (!config.streaming || !ready()) return;
        if (!config.seat) accept_input(0, frame, buttons);
        else {
            RecoveryBytes body;
            put(body, buttons);
            links[0]->queue(StreamInput, epoch, frame, body);
        }
    }
    Impl(RecoveryConfig c, RecoveryCallbacks cb) : config(std::move(c)), callbacks(std::move(cb)) {
        if (config.players < 2 || config.players > 4 || config.seat >= config.players ||
            !config.session || config.identity.empty() || config.identity.size() > 2048)
            throw std::runtime_error("invalid recovery room configuration");
        std::filesystem::create_directories(config.directory);
        storage_lock = CreateFileW((config.directory / L"room.lock").c_str(), GENERIC_WRITE, 0,
                                   nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (storage_lock == INVALID_HANDLE_VALUE)
            throw std::runtime_error("recovery seat already open in this directory");
        checkpoint = config.directory / L"confirmed.state";
        journal = config.directory / L"inputs.journal";
        meta = config.directory / L"seats.meta";
        if (config.seat == 0) {
            load_metadata();
            recover_storage();
            auto address = endpoint(config.bind, true);
            listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (listener == INVALID_SOCKET)
                throw std::runtime_error("cannot create recovery listener");
            // A single live authority owns the port; do not allow SO_REUSEADDR hijacking.
            int yes = 1;
            setsockopt(listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<char *>(&yes),
                       sizeof(yes));
            if (bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) ||
                listen(listener, 8))
                throw std::runtime_error("cannot bind recovery TCP port");
            nonblocking(listener);
            owners[0] = callbacks.owner_save;
        } else {
            auto path = config.directory / L"seat.key";
            if (std::filesystem::exists(path)) {
                auto b = read_file(path, 32);
                credential = {b.begin(), b.end()};
            } else {
                credential = random_token();
                atomic_write(path, {reinterpret_cast<const std::uint8_t *>(credential.data()),
                                    credential.size()});
            }
            if (credential.size() != 32)
                throw std::runtime_error("invalid recovery seat credential");
        }
        committed = callbacks.capture();
        committed_hash = digest(committed);
    }
    ~Impl() {
        links.clear();
        if (listener != INVALID_SOCKET)
            closesocket(listener);
        if (storage_lock != INVALID_HANDLE_VALUE)
            CloseHandle(storage_lock);
    }
    void load_metadata() {
        if (!std::filesystem::exists(meta))
            return;
        auto b = read_file(meta, 4096);
        if (b.size() < 20 || digest(std::span<const std::uint8_t>(b).first(b.size() - 20)) !=
                                 Reader{std::span<const std::uint8_t>(b).last(20)}.digest())
            throw std::runtime_error("corrupt recovery seat metadata");
        Reader r{std::span<const std::uint8_t>(b).first(b.size() - 20)};
        if (r.word() != magic || r.word() != version || r.text() != config.identity ||
            r.word() != config.session || r.word() != config.players)
            throw std::runtime_error("recovery metadata identity mismatch");
        for (auto &token : credentials)
            token = r.text(32);
        r.done();
    }
    void save_metadata() {
        RecoveryBytes b;
        put(b, magic);
        put(b, version);
        text(b, config.identity);
        put(b, config.session);
        put(b, config.players);
        for (auto &token : credentials)
            text(b, token);
        append_digest(b, digest(b));
        atomic_write(meta, b);
    }
    void save_checkpoint() {
        RecoveryBytes b;
        put(b, magic);
        put(b, version);
        text(b, config.identity);
        put(b, config.session);
        put(b, config.players);
        put(b, tick);
        append_digest(b, committed_hash);
        blob(b, committed);
        append_digest(b, digest(b));
        atomic_write(checkpoint, b);
        durable_write(journal, {});
    }
    void recover_storage() {
        if (!std::filesystem::exists(checkpoint))
            return;
        auto b = read_file(checkpoint, max_body + 4096);
        if (b.size() < 20 || digest(std::span<const std::uint8_t>(b).first(b.size() - 20)) !=
                                 Reader{std::span<const std::uint8_t>(b).last(20)}.digest())
            throw std::runtime_error("corrupt recovery checkpoint envelope");
        Reader r{std::span<const std::uint8_t>(b).first(b.size() - 20)};
        if (r.word() != magic || r.word() != version || r.text() != config.identity ||
            r.word() != config.session || r.word() != config.players)
            throw std::runtime_error("recovery checkpoint belongs to a different build/room");
        tick = r.word();
        committed_hash = r.digest();
        committed = r.blob();
        r.done();
        if (digest(committed) != committed_hash)
            throw std::runtime_error("corrupt recovery checkpoint");
        restore_committed();
        initialized = true;
        if (std::filesystem::exists(journal)) {
            auto rows = read_file(journal, 1024 * 1024);
            constexpr std::size_t row_size = 52;
            auto complete = rows.size() / row_size * row_size;
            for (std::size_t off = 0; off < complete; off += row_size) {
                auto row = std::span<const std::uint8_t>(rows).subspan(off, row_size);
                Reader item{row};
                auto next = item.word();
                std::array<std::uint16_t, 4> buttons{};
                auto a = item.word(), b = item.word();
                buttons = {std::uint16_t(a), std::uint16_t(a >> 16), std::uint16_t(b),
                           std::uint16_t(b >> 16)};
                auto hash = item.digest(), checksum = item.digest();
                item.done();
                if (digest(row.first(32)) != checksum)
                    throw std::runtime_error("corrupt recovery input journal");
                if (next <= tick)
                    continue;
                if (next != tick + 1)
                    throw std::runtime_error("gap in recovery input journal");
                callbacks.advance({buttons.data(), config.players});
                auto state = callbacks.capture();
                if (digest(state) != hash)
                    throw std::runtime_error("recovery journal replay diverged");
                committed = std::move(state);
                committed_hash = hash;
                tick = next;
            }
            // An interrupted final write cannot become a committed input row.
            if (complete != rows.size()) {
                rows.resize(complete);
                durable_write(journal, rows);
            }
        }
        restore_committed();
    }
    void persist_commit() {
        RecoveryBytes row;
        put(row, tick);
        put(row, std::uint32_t(inputs[0]) | (std::uint32_t(inputs[1]) << 16));
        put(row, std::uint32_t(inputs[2]) | (std::uint32_t(inputs[3]) << 16));
        append_digest(row, committed_hash);
        append_digest(row, digest(row));
        durable_write(journal, row, true);
        if (tick % 120 == 0)
            save_checkpoint();
    }
    void broadcast(Type type, const RecoveryBytes &body = {}) {
        for (unsigned s = 1; s < config.players; ++s)
            if (seats[s])
                seats[s]->queue(type, epoch, tick, body);
    }
    bool all(bool Connection::*flag) const {
        for (unsigned s = 1; s < config.players; ++s)
            if (!seats[s] || !(seats[s]->*flag))
                return false;
        return true;
    }
    void synchronize() {
        if (!initialized)
            return;
        restore_committed();
        future_inputs.clear();
        recent_hashes.clear();
        candidate.clear();
        awaiting_input = false;
        awaiting_ack = false;
        ending = false;
        if (++epoch == 0)
            throw std::runtime_error("recovery generation overflow");
        RecoveryBytes body;
        append_digest(body, committed_hash);
        blob(body, committed);
        for (unsigned s = 1; s < config.players; ++s)
            if (seats[s]) {
                auto &p = *seats[s];
                p.ready = p.input = p.ack = p.ended = false;
                p.verified = tick;
                // Drop unsent messages from the previous generation only after a
                // connection loss? Keep framing intact: queued old frames are
                // ignored by epoch, and State always follows complete frames.
                p.queue(State, epoch, tick, body);
            }
        phase_started = Clock::now();
    }
    void disconnect(Connection &c) {
        if (c.seat > 0 && seats[c.seat] == &c) {
            seats[c.seat] = nullptr;
            c.close();
            synchronize();
        } else
            c.close();
    }
    void receive_host(Connection &c, const Packet &p) {
        Reader r{p.body};
        if (p.type == Hello) {
            auto identity = r.text();
            auto session = r.word(), players = r.word(), seat = r.word();
            auto token = r.text(32);
            auto owner = r.blob(128 * 1024);
            r.done();
            if (identity != config.identity || session != config.session ||
                players != config.players || !seat || seat >= config.players ||
                token.size() != 32 || (!credentials[seat].empty() && credentials[seat] != token)) {
                RecoveryBytes error;
                text(error, "incompatible room, build or seat credential");
                c.queue(Rejected, epoch, tick, error);
                c.seat = -2;
                return;
            }
            if (c.seat != -1)
                throw std::runtime_error("duplicate recovery hello");
            if (seats[seat]) {
                auto *old = seats[seat];
                seats[seat] = nullptr;
                old->close();
            }
            credentials[seat] = token;
            save_metadata();
            c.seat = static_cast<int>(seat);
            seats[seat] = &c;
            owners[seat] = std::move(owner);
            if (!initialized) {
                bool present = true;
                for (unsigned s = 1; s < config.players; ++s)
                    present &= seats[s] != nullptr;
                if (!present)
                    return;
                if (callbacks.install_owner)
                    for (unsigned s = 0; s < config.players; ++s)
                        callbacks.install_owner(s, owners[s]);
                committed = callbacks.capture();
                committed_hash = digest(committed);
                initialized = true;
                save_checkpoint();
            }
            synchronize();
            return;
        }
        if (p.type == Ping) {
            c.queue(Pong, epoch, tick);
            return;
        }
        if (p.type == Pong)
            return;
        if (c.seat < 1)
            throw std::runtime_error("recovery message before hello");
        if (p.epoch != epoch)
            return;
        if (p.type == Ready) {
            auto hash = r.digest();
            r.done();
            if (p.tick != tick || hash != committed_hash)
                throw std::runtime_error("recovery ready digest mismatch");
            c.ready = true;
            c.verified = tick;
        } else if (p.type == StreamInput && config.streaming && c.ready) {
            auto buttons = r.word();
            r.done();
            if (buttons > 1023) throw std::runtime_error("invalid streaming buttons");
            accept_input(c.seat, p.tick, static_cast<std::uint16_t>(buttons));
        } else if (p.type == StreamHash && config.streaming && c.ready) {
            auto hash = r.digest();
            r.done();
            auto expected = recent_hashes.find(p.tick);
            if (p.tick <= c.verified) return;
            if (expected == recent_hashes.end())
                throw std::runtime_error("streaming acknowledgement outside history");
            if (hash != expected->second) {
                std::fprintf(stderr, "recovery divergence seat=%d tick=%u; restoring host state\n", c.seat, p.tick);
                if (++repairs > 3) {
                    fatal_error = "persistent streaming state divergence";
                    throw std::runtime_error(fatal_error);
                }
                synchronize();
                return;
            }
            c.verified = p.tick;
        } else if (p.type == Input && awaiting_input) {
            auto buttons = r.word();
            r.done();
            if (p.tick != tick || buttons > 1023 || c.input)
                throw std::runtime_error("invalid recovery input tick");
            inputs[c.seat] = static_cast<std::uint16_t>(buttons);
            c.input = true;
        } else if (p.type == Ack && awaiting_ack) {
            auto hash = r.digest();
            r.done();
            if (p.tick != tick + 1)
                throw std::runtime_error("invalid recovery acknowledgement tick");
            if (hash != candidate_hash) {
                if (++repairs > 3) {
                    fatal_error = "persistent recovery state divergence";
                    throw std::runtime_error(fatal_error);
                }
                synchronize();
                return;
            }
            c.ack = true;
        } else if (p.type == EndAck && ending) {
            r.done();
            if (p.tick != tick)
                throw std::runtime_error("invalid recovery end tick");
            c.ended = true;
        } else
            throw std::runtime_error("unexpected recovery message for current phase");
    }
    bool receive_guest(Connection &c, const Packet &p, std::uint16_t buttons) {
        Reader r{p.body};
        if (p.type == Ping) {
            c.queue(Pong, epoch, tick);
            return false;
        }
        if (p.type == Pong)
            return false;
        if (p.type == Rejected)
            throw std::runtime_error("recovery host rejected connection: " + r.text());
        if (p.type == State) {
            auto hash = r.digest();
            auto state = r.blob();
            r.done();
            if (digest(state) != hash)
                throw std::runtime_error("invalid recovery state digest");
            committed = std::move(state);
            committed_hash = hash;
            tick = p.tick;
            restore_committed();
            epoch = p.epoch;
            initialized = true;
            c.ready = true;
            candidate.clear();
            awaiting_ack = false;
            ending = false;
            RecoveryBytes ack;
            append_digest(ack, hash);
            c.queue(Ready, epoch, tick, ack);
            return false;
        }
        if (!c.ready || p.epoch != epoch)
            return false;
        if (p.type == StreamFrame && config.streaming) {
            if (p.tick != tick + 1) throw std::runtime_error("streaming frame gap");
            for (unsigned s = 0; s < config.players; ++s) {
                auto key = r.word();
                if (key > 1023) throw std::runtime_error("invalid streaming input");
                inputs[s] = static_cast<std::uint16_t>(key);
            }
            auto expected = r.digest();
            r.done();
            callbacks.advance({inputs.data(), config.players});
            auto state = callbacks.capture();
            auto hash = digest(state);
            RecoveryBytes body;
            append_digest(body, hash);
            c.queue(StreamHash, epoch, p.tick, body);
            if (hash != expected) {
                c.ready = false;
                restore_committed();
                return false;
            }
            committed = std::move(state);
            committed_hash = hash;
            tick = p.tick;
            return true;
        } else if (p.type == AskInput) {
            r.done();
            if (p.tick != tick || !candidate.empty())
                throw std::runtime_error("invalid recovery input request");
            RecoveryBytes input;
            put(input, (callbacks.sample_input ? callbacks.sample_input(tick) : buttons) & 1023);
            c.queue(Input, epoch, tick, input);
        } else if (p.type == Advance) {
            if (p.tick != tick || !candidate.empty())
                throw std::runtime_error("invalid recovery advance tick");
            for (unsigned s = 0; s < config.players; ++s) {
                auto key = r.word();
                if (key > 1023)
                    throw std::runtime_error("invalid recovery key word");
                inputs[s] = static_cast<std::uint16_t>(key);
            }
            r.done();
            callbacks.advance({inputs.data(), config.players});
            candidate = callbacks.capture();
            candidate_hash = digest(candidate);
            RecoveryBytes ack;
            append_digest(ack, candidate_hash);
            c.queue(Ack, epoch, tick + 1, ack);
        } else if (p.type == Commit) {
            auto hash = r.digest();
            r.done();
            if (p.tick != tick + 1 || candidate.empty() || hash != candidate_hash)
                throw std::runtime_error("invalid recovery commit");
            committed = std::move(candidate);
            committed_hash = hash;
            tick = p.tick;
            return true;
        } else if (p.type == End) {
            r.done();
            if (p.tick != tick || !candidate.empty())
                throw std::runtime_error("uncommitted recovery end");
            ending = true;
            c.queue(EndAck, epoch, tick);
        } else
            throw std::runtime_error("unexpected recovery host message");
        return false;
    }
    bool poll(std::uint16_t buttons, bool allow) {
        if (!fatal_error.empty())
            throw std::runtime_error(fatal_error);
        bool advanced = false;
        if (config.seat == 0) {
            for (unsigned i = 0; i < 8; ++i) {
                auto s = accept(listener, nullptr, nullptr);
                if (s == INVALID_SOCKET)
                    break;
                if (links.size() >= 8) {
                    closesocket(s);
                    continue;
                }
                tune(s);
                links.push_back(std::make_unique<Connection>(s));
            }
            for (auto &link : links) {
                auto &c = *link;
                if (!c.pump()) {
                    disconnect(c);
                    continue;
                }
                Packet p;
                unsigned count = 0;
                try {
                    while (count++ < 32 && c.next(p))
                        receive_host(c, p);
                    c.heartbeat(epoch, tick);
                } catch (const std::exception &) {
                    disconnect(c);
                }
            }
            std::erase_if(links, [](const auto &c) { return c->socket == INVALID_SOCKET; });
            if (!initialized || !all(&Connection::ready))
                return false;
            if (ending) {
                if (all(&Connection::ended)) {
                    bool flushed = true;
                    for (unsigned s = 1; s < config.players; ++s)
                        flushed &= seats[s]->tx.empty();
                    if (flushed)
                        done = true;
                }
                return false;
            }
            if (config.finish_tick && tick >= config.finish_tick && !awaiting_ack &&
                !awaiting_input) {
                if (config.streaming)
                    for (unsigned s = 1; s < config.players; ++s)
                        if (seats[s]->verified < tick) return false;
                save_checkpoint();
                broadcast(End);
                ending = true;
                return false;
            }
            if (config.streaming) {
                auto it = future_inputs.find(tick);
                if (it == future_inputs.end() || it->second.mask != (1u << config.players) - 1)
                    return false;
                // Bound outstanding verification independently of prediction.
                for (unsigned s = 1; s < config.players; ++s)
                    if (tick - seats[s]->verified >= 120) return false;
                inputs = it->second.keys;
                future_inputs.erase(it);
                callbacks.advance({inputs.data(), config.players});
                committed = callbacks.capture();
                committed_hash = digest(committed);
                ++tick;
                recent_hashes[tick] = committed_hash;
                while (recent_hashes.size() > 128) recent_hashes.erase(recent_hashes.begin());
                persist_commit();
                RecoveryBytes body;
                for (unsigned s = 0; s < config.players; ++s) put(body, inputs[s]);
                append_digest(body, committed_hash);
                broadcast(StreamFrame, body);
                return true;
            }
            if (awaiting_input && all(&Connection::input)) {
                RecoveryBytes row;
                for (unsigned s = 0; s < config.players; ++s)
                    put(row, inputs[s]);
                broadcast(Advance, row);
                callbacks.advance({inputs.data(), config.players});
                candidate = callbacks.capture();
                candidate_hash = digest(candidate);
                awaiting_input = false;
                awaiting_ack = true;
                phase_started = Clock::now();
            }
            if (awaiting_ack && all(&Connection::ack)) {
                committed = std::move(candidate);
                committed_hash = candidate_hash;
                ++tick;
                persist_commit();
                repairs = 0;
                RecoveryBytes body;
                append_digest(body, committed_hash);
                broadcast(Commit, body);
                awaiting_ack = false;
                advanced = true;
            }
            if (allow && !advanced && !awaiting_input && !awaiting_ack) {
                inputs[0] =
                    (callbacks.sample_input ? callbacks.sample_input(tick) : buttons) & 1023;
                for (unsigned s = 1; s < config.players; ++s)
                    seats[s]->input = seats[s]->ack = false;
                broadcast(AskInput);
                awaiting_input = true;
                phase_started = Clock::now();
            }
        } else {
            if (links.empty() && Clock::now() >= retry) {
                retry = Clock::now() + std::chrono::seconds(1);
                sockaddr_in address{};
                try {
                    address = endpoint(config.peer, false);
                } catch (const std::exception &) {
                    return false;
                } // Wi-Fi/DNS may be temporarily unavailable.
                auto s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (s == INVALID_SOCKET)
                    return false;
                tune(s);
                auto c = std::make_unique<Connection>(s);
                int result = connect(s, reinterpret_cast<sockaddr *>(&address), sizeof(address));
                if (result && WSAGetLastError() != WSAEWOULDBLOCK) {
                    c->close();
                    return false;
                }
                c->connecting = result != 0;
                RecoveryBytes hello;
                text(hello, config.identity);
                put(hello, config.session);
                put(hello, config.players);
                put(hello, config.seat);
                text(hello, credential);
                blob(hello, callbacks.owner_save);
                c->queue(Hello, 0, 0, hello);
                links.push_back(std::move(c));
            }
            if (links.empty())
                return false;
            auto &c = *links[0];
            if (!c.pump()) {
                if (initialized)
                    restore_committed();
                candidate.clear();
                links.clear();
                ending = false;
                return false;
            }
            Packet p;
            unsigned count = 0;
            while (count++ < 32 && c.next(p))
                advanced = receive_guest(c, p, buttons) || advanced;
            c.heartbeat(epoch, tick);
            if (ending && c.tx.empty())
                done = true;
        }
        return advanced;
    }
    std::string status() const {
        if (done)
            return "Sesión guardada";
        if (!config.seat) {
            unsigned waiting = 0;
            for (unsigned s = 1; s < config.players; ++s)
                if (!seats[s] || !seats[s]->ready)
                    waiting |= 1u << s;
            if (waiting) {
                std::string s = "Esperando reconexión:";
                for (unsigned i = 1; i < config.players; ++i)
                    if (waiting & (1u << i))
                        s += " jugador " + std::to_string(i + 1);
                return s;
            }
        } else if (links.empty() || !links[0]->ready)
            return "Reconectando al host";
        if ((awaiting_ack || awaiting_input) &&
            Clock::now() - phase_started > std::chrono::seconds(2))
            return "Esperando confirmación de controles";
        return "Online recuperable - jugador " + std::to_string(config.seat + 1);
    }
};
RecoveryRoom::RecoveryRoom(RecoveryConfig c, RecoveryCallbacks callbacks)
    : impl_(std::make_unique<Impl>(std::move(c), std::move(callbacks))) {}
RecoveryRoom::~RecoveryRoom() = default;
bool RecoveryRoom::poll(std::uint16_t b, bool allow) { return impl_->poll(b, allow); }
std::uint32_t RecoveryRoom::tick() const { return impl_->tick; }
bool RecoveryRoom::ready() const { return impl_->ready(); }
void RecoveryRoom::submit_input(std::uint32_t tick, std::uint16_t buttons) {
    impl_->submit_input(tick, buttons);
}
bool RecoveryRoom::finished() const { return impl_->done; }
std::string RecoveryRoom::status() const { return impl_->status(); }
} // namespace ss2
