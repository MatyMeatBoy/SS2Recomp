#include "ss2_extended_view.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "gba_bus.h"
#include "gba_ppu.h"
#include "runtime_bus_bridge.h"

extern "C" unsigned g_ws_extra_left;
extern "C" unsigned g_ws_extra_right;

namespace ss2 {
namespace {

// Field engine state (Shining Soul II EU, AU2P rev 0), established from
// PrimatesGBA dumps of the opening field (see re/NOTES.md):
//
//   BG0      HUD and dialog plane: char block 2, screen block 28.
//   BG1..3   field planes: char block 0, screen blocks 29..31, 256x256 rings.
//   0x03003188 + 24*k (k = 0..2) describes BG(k+1):
//     +0x0C  u32  row-major map of hardware BG entries in EWRAM or ROM
//     +0x10  u16  width in tiles
//     +0x12  u16  height in tiles
//     +0x14  u16  BG number (1..3)
//
// The authored maps hold the same entries the game streams into the rings,
// so margins sample real field data instead of repeating the ring.
constexpr std::uint32_t kLayerTable = 0x03003188u;
constexpr std::uint32_t kLayerStride = 24u;
constexpr int kMaxMapTiles = 256;
constexpr std::uint32_t kEwramBase = 0x02000000u;
constexpr std::uint32_t kEwramSize = 0x40000u;
constexpr std::uint32_t kIwramBase = 0x03000000u;
constexpr std::uint32_t kIwramSize = 0x8000u;

struct Layer {
    const std::uint8_t* map = nullptr;  // validated authored map, RAM or ROM
    int width = 0;
    int height = 0;
    int camera_x = 0;
    int camera_y = 0;
    bool valid = false;
};

struct FieldFrame {
    const gba::GbaBus* bus = nullptr;
    unsigned long long vblank = ~0ull;
    bool field = false;
    Layer layers[4];
};

FieldFrame g_frame;
bool g_equipment_widescreen = false;
std::uint16_t read16(const std::uint8_t* p, std::uint32_t offset);

bool equipment_registers(const std::uint8_t* io) {
    // Inventory's scrolling decorative BG3 is an intentionally repeating
    // 256x256 texture. BG2 contains cells and labels and must never repeat.
    return g_equipment_widescreen && read16(io, 0) == 0x1D40 &&
           read16(io, 0x08) == 0x1C08 && read16(io, 0x0C) == 0x9A01 &&
           read16(io, 0x0E) == 0x1F03;
}

struct HudRect {
    int x0;
    int y0;
    int x1;
    int y1;
};

// BG0 HUD groups in native 240x160 space. The dialog box and its portrait
// stay centered; only the edge-attached status and slot panels move.
constexpr HudRect kLeftHud[] = {
    {0, 0, 176, 16},   // HP / SP / SOUL status bar
    {0, 16, 64, 48},   // weapon slots (L)
};
constexpr HudRect kRightHud[] = {
    {176, 16, 240, 48},  // item slots (R)
};

std::uint16_t read16(const std::uint8_t* p, std::uint32_t offset) {
    return static_cast<std::uint16_t>(
        p[offset] | (static_cast<std::uint16_t>(p[offset + 1]) << 8));
}

std::uint32_t iwram32(gba::GbaBus* bus, std::uint32_t address) {
    const std::uint32_t off = address - kIwramBase;
    if (off + 4u > kIwramSize) return 0;
    const std::uint8_t* p = bus->iwram_ptr() + off;
    return static_cast<std::uint32_t>(p[0]) |
        (static_cast<std::uint32_t>(p[1]) << 8) |
        (static_cast<std::uint32_t>(p[2]) << 16) |
        (static_cast<std::uint32_t>(p[3]) << 24);
}

std::uint16_t map_entry(gba::GbaBus* bus, const Layer& layer, int tx, int ty) {
    const std::uint32_t off = static_cast<std::uint32_t>((ty * layer.width + tx) * 2);
    return read16(layer.map, off);
}

std::uint16_t ring_entry(gba::GbaBus* bus, const std::uint8_t* io, int bg,
                         int screen_x, int screen_y) {
    const std::uint16_t cnt = read16(io, 0x08u + static_cast<std::uint32_t>(bg * 2));
    const std::uint32_t base = static_cast<std::uint32_t>((cnt >> 8) & 0x1Fu) * 0x800u;
    const int hofs = read16(io, 0x10u + static_cast<std::uint32_t>(bg * 4)) & 0x1FF;
    const int vofs = read16(io, 0x12u + static_cast<std::uint32_t>(bg * 4)) & 0x1FF;
    const int tx = ((screen_x + hofs) & 0xFF) >> 3;
    const int ty = ((screen_y + vofs) & 0xFF) >> 3;
    return read16(bus->vram_ptr(), base + static_cast<std::uint32_t>((ty * 32 + tx) * 2));
}

// The opening field keeps BG0 for the HUD and BG1..BG3 for the field rings.
// Title, intro, menus and transitions use other layouts or fail the camera
// match below, so they keep the native view.
bool field_registers(const std::uint8_t* io) {
    const std::uint16_t dispcnt = read16(io, 0x00);
    if ((dispcnt & 0x0007u) != 0) return false;
    if ((dispcnt & 0x1100u) != 0x1100u) return false;  // BG0 HUD + OBJ
    const std::uint16_t bg0 = read16(io, 0x08);
    if (((bg0 >> 8) & 0x1Fu) != 28u || ((bg0 >> 2) & 3u) != 2u) return false;
    for (int bg = 1; bg <= 3; ++bg) {
        const std::uint16_t cnt = read16(io, 0x08u + static_cast<std::uint32_t>(bg * 2));
        if (((cnt >> 8) & 0x1Fu) != static_cast<unsigned>(28 + bg) ||
            ((cnt >> 2) & 3u) != 0u || ((cnt >> 14) & 3u) != 0u ||
            (cnt & 0x0080u) != 0u) {
            return false;
        }
    }
    return true;
}

// The status bar occupies BG0 row 0, columns 0..21, in every field frame
// (including dialog). Menus that replace the field also replace BG1..BG3,
// which the camera match rejects.
bool status_bar_present(gba::GbaBus* bus) {
    const std::uint8_t* vram = bus->vram_ptr();
    constexpr std::uint32_t base = 28u * 0x800u;
    for (int col = 0; col < 22; ++col) {
        if ((read16(vram, base + static_cast<std::uint32_t>(col * 2)) & 0x03FFu) == 0)
            return false;
    }
    return true;
}

bool read_layer(gba::GbaBus* bus, int bg, Layer& layer) {
    const std::uint32_t desc = kLayerTable + static_cast<std::uint32_t>(bg - 1) * kLayerStride;
    const std::uint32_t map = iwram32(bus, desc + 0x0Cu);
    const std::uint32_t size = iwram32(bus, desc + 0x10u);
    const std::uint32_t number = iwram32(bus, desc + 0x14u) & 0xFFFFu;
    const int width = static_cast<int>(size & 0xFFFFu);
    const int height = static_cast<int>(size >> 16);
    if (number != static_cast<std::uint32_t>(bg) || width <= 0 || height <= 0 ||
        width > kMaxMapTiles || height > kMaxMapTiles ||
        (map & 1u) != 0u) {
        return false;
    }
    const std::uint32_t bytes = static_cast<std::uint32_t>(width * height * 2);
    if (map >= kEwramBase && map < kEwramBase + kEwramSize) {
        const std::uint32_t off = map - kEwramBase;
        if (bytes > kEwramSize - off) return false;
        layer.map = bus->ewram_ptr() + off;
    } else if (map >= 0x08000000u && map < 0x0A000000u) {
        // Uncompressed fields (including the state-4 castle) reference ROM
        // directly; the opening field instead decompresses its maps to RAM.
        const std::uint32_t off = map - 0x08000000u;
        if (!bus->rom_ptr() || off >= bus->rom_size() ||
            bytes > bus->rom_size() - off) return false;
        layer.map = bus->rom_ptr() + off;
    } else {
        return false;
    }
    layer.width = width;
    layer.height = height;
    return true;
}

// Scores a camera hypothesis by comparing the live ring with the authored map
// at distributed visible points. Returns matches; *total counts in-map probes.
int score_camera(gba::GbaBus* bus, const std::uint8_t* io, int bg,
                 const Layer& layer, int camera_x, int camera_y, int* total) {
    constexpr int probes_x[] = {4, 44, 84, 124, 164, 204, 236};
    constexpr int probes_y[] = {20, 44, 68, 92, 116, 140, 156};
    int matches = 0;
    *total = 0;
    for (int y : probes_y) {
        for (int x : probes_x) {
            const int wx = camera_x + x;
            const int wy = camera_y + y;
            if (wx < 0 || wy < 0 || wx >= layer.width * 8 || wy >= layer.height * 8)
                continue;
            ++*total;
            if (map_entry(bus, layer, wx >> 3, wy >> 3) == ring_entry(bus, io, bg, x, y))
                ++matches;
        }
    }
    return matches;
}

// The engine keeps world tile (x, y) in ring cell (x & 31, y & 31), so the
// camera is congruent to the scroll registers modulo the 256-pixel ring.
// Each candidate must reproduce the visible ring before margins trust it.
bool resolve_camera(gba::GbaBus* bus, const std::uint8_t* io, int bg, Layer& layer) {
    const int hofs = read16(io, 0x10u + static_cast<std::uint32_t>(bg * 4)) & 0xFF;
    const int vofs = read16(io, 0x12u + static_cast<std::uint32_t>(bg * 4)) & 0xFF;
    // sub_08009594 writes scroll + global offset + per-layer offset to the
    // hardware registers. Prefer that full coordinate over an ambiguous match
    // in a repeated tile pattern, but only while it agrees with the live ring.
    const auto* ram = bus->iwram_ptr();
    const auto desc = 0x3188u + static_cast<unsigned>(bg - 1) * kLayerStride;
    const int cx = static_cast<std::int16_t>(read16(ram, 0x31EC)) +
                   static_cast<std::int16_t>(read16(ram, 0x31F4)) +
                   static_cast<std::int16_t>(read16(ram, desc + 4));
    const int cy = static_cast<std::int16_t>(read16(ram, 0x31EE)) +
                   static_cast<std::int16_t>(read16(ram, 0x31F6)) +
                   static_cast<std::int16_t>(read16(ram, desc + 6));
    if ((cx & 255) == hofs && (cy & 255) == vofs) {
        int total = 0;
        const int score = score_camera(bus, io, bg, layer, cx, cy, &total);
        if (total >= 16 && score * 4 >= total * 3) {
            layer.camera_x = cx; layer.camera_y = cy;
            return true;
        }
    }
    int best = -1;
    int best_total = 0;
    for (int cy = vofs; cy < layer.height * 8; cy += 256) {
        for (int cx = hofs; cx < layer.width * 8; cx += 256) {
            int total = 0;
            const int score = score_camera(bus, io, bg, layer, cx, cy, &total);
            if (total >= 16 && score * 4 >= total * 3 && score > best) {
                best = score;
                best_total = total;
                layer.camera_x = cx;
                layer.camera_y = cy;
            }
        }
    }
    return best >= 0 && best_total > 0;
}

void trace_frame_once(const FieldFrame& frame, const std::uint8_t* io) {
    // Providers are hot; read the environment exactly once.
    static const bool enabled = [] {
        const char* value = std::getenv("GBARECOMP_SS2_WS_TRACE");
        return value && *value && *value != '0';
    }();
    if (!enabled) return;
    static int previous = -1;
    const int state = frame.field ? 1 : 0;
    if (state == previous) return;
    previous = state;
    std::fprintf(stderr, "[ss2:adaptive-view] field=%d DISPCNT=%04X BG=%04X/%04X/%04X/%04X",
                 state, read16(io, 0x00), read16(io, 0x08), read16(io, 0x0A),
                 read16(io, 0x0C), read16(io, 0x0E));
    for (int bg = 1; bg <= 3; ++bg) {
        const Layer& l = frame.layers[bg];
        std::fprintf(stderr, " L%d=%dx%d@(%d,%d)%s", bg, l.width, l.height,
                     l.camera_x, l.camera_y, l.valid ? "" : "!");
    }
    std::fprintf(stderr, "\n");
}

const FieldFrame& current_frame(gba::GbaBus* bus) {
    if (g_frame.bus == bus && g_frame.vblank == g_runtime_vblank_starts) return g_frame;
    g_frame = {};
    g_frame.bus = bus;
    g_frame.vblank = g_runtime_vblank_starts;
    if (!bus) return g_frame;
    const std::uint8_t* io = bus->io().raw();
    if (field_registers(io) && status_bar_present(bus)) {
        const std::uint16_t dispcnt = read16(io, 0x00);
        bool any = false;
        for (int bg = 1; bg <= 3; ++bg) {
            if (!(dispcnt & (0x0100u << bg))) continue;
            Layer& layer = g_frame.layers[bg];
            layer.valid = read_layer(bus, bg, layer) && resolve_camera(bus, io, bg, layer);
            any = any || layer.valid;
        }
        g_frame.field = any;
    }
    trace_frame_once(g_frame, io);
    return g_frame;
}

int field_tilemap_provider(int bg, int hw_x, int screen_y, std::uint16_t* out_entry) {
    gba::GbaBus* bus = gbarecomp::active_bus();
    if (!bus || !out_entry || bg < 1 || bg > 3) return gba::kWsTilemapUnavailable;
    if (bg == 3 && equipment_registers(bus->io().raw()))
        return gba::kWsTilemapKeepWrapped;
    const FieldFrame& frame = current_frame(bus);
    const Layer& layer = frame.layers[bg];
    if (!frame.field || !layer.valid) return gba::kWsTilemapUnavailable;
    const int wx = layer.camera_x + hw_x;
    const int wy = layer.camera_y + screen_y;
    if (wx < 0 || wy < 0 || wx >= layer.width * 8 || wy >= layer.height * 8)
        return gba::kWsTilemapUnavailable;
    *out_entry = map_entry(bus, layer, wx >> 3, wy >> 3);
    return gba::kWsTilemapReplace;
}

bool inside(const HudRect& rect, int x, int y) {
    return x >= rect.x0 && x < rect.x1 && y >= rect.y0 && y < rect.y1;
}

int hud_bg_x_provider(int bg, int output_x, int screen_y, int* out_hw_x) {
    gba::GbaBus* bus = gbarecomp::active_bus();
    if (bg == 3 && out_hw_x && bus && equipment_registers(bus->io().raw())) {
        const int x = output_x - static_cast<int>(g_ws_extra_left);
        if (x < 0 || x >= 240) {
            // Reflect the native decorative plane at its exact pixel edges.
            // -1 maps to 0, 240 maps to 239; scroll and animation stay intact.
            *out_hw_x = x < 0 ? -x - 1 : 479 - x;
            return 1;
        }
        return 0;
    }
    if (bg != 0 || !out_hw_x || !bus || !current_frame(bus).field) return 0;

    const int hw_x = output_x - static_cast<int>(g_ws_extra_left);
    const int left_shift = -static_cast<int>(g_ws_extra_left);
    const int right_shift = static_cast<int>(g_ws_extra_right);
    for (const HudRect& rect : kLeftHud) {
        const HudRect destination = {rect.x0 + left_shift, rect.y0,
                                     rect.x1 + left_shift, rect.y1};
        if (inside(destination, hw_x, screen_y)) {
            *out_hw_x = hw_x - left_shift;
            return 1;
        }
    }
    for (const HudRect& rect : kRightHud) {
        const HudRect destination = {rect.x0 + right_shift, rect.y0,
                                     rect.x1 + right_shift, rect.y1};
        if (inside(destination, hw_x, screen_y)) {
            *out_hw_x = hw_x - right_shift;
            return 1;
        }
    }
    // Remove the original copy after it has moved; the field and centered UI
    // continue through their own layers.
    if (left_shift != 0) {
        for (const HudRect& rect : kLeftHud)
            if (inside(rect, hw_x, screen_y)) return -1;
    }
    if (right_shift != 0) {
        for (const HudRect& rect : kRightHud)
            if (inside(rect, hw_x, screen_y)) return -1;
    }
    return 0;
}

// Slot panels draw their icons and cursor as priority-0 OBJs over the BG0
// panels (y < 48). World actors use priority 2 and stay camera-relative.
int hud_obj_x_provider(int, std::uint16_t attr0, std::uint16_t attr1,
                       std::uint16_t attr2, int* out_x) {
    gba::GbaBus* bus = gbarecomp::active_bus();
    if (!out_x || !bus || !current_frame(bus).field) return 0;
    const int y = attr0 & 0x00FF;
    int x = attr1 & 0x01FF;
    // The game's OBJ emitters accept world X in [-64, 240]. A constituent
    // sprite can extend to X=304; 256..304 is right of the viewport, whereas
    // 448..511 encodes negative X. Signed 9-bit decoding incorrectly wraps
    // visible right-margin sprites at 256.
    if (x >= 384) x -= 512;
    if (((attr2 >> 10) & 3) != 0 || y >= 48 || (attr0 & 0x0300u) == 0x0200u) {
        *out_x = x;
        return 1;
    }
    *out_x = x < 120 ? x - static_cast<int>(g_ws_extra_left)
                     : x + static_cast<int>(g_ws_extra_right);
    return 1;
}

int transition_window_provider(const std::uint8_t* io, int hw_x, int y,
                               std::uint16_t* out_mask) {
    if (equipment_registers(io)) {
        // Only the decorative plane may occupy inventory margins. The native
        // ring and offscreen OBJ entries can contain other menu pages/icons.
        *out_mask = (1u << 3) | (1u << 5);
        return 1;
    }
    // SS2 closes scenes using WIN0 (inside=0, outside=all layers), not by
    // fading only the palette. Continue that mask to the expanded edges.
    // Use the PPU's scanline IO snapshot: the guest may update it mid-frame.
    const auto dispcnt = read16(io, 0);
    const auto inside = read16(io, 0x48) & 0x3Fu;
    const auto outside = read16(io, 0x4A) & 0x3Fu;
    if (!(dispcnt & 0x2000u) || inside != 0 || outside != 0x3Fu) return 0;
    const auto h = read16(io, 0x40), v = read16(io, 0x44);
    const int left = h >> 8, right = h & 255;
    const int top = v >> 8, bottom = v & 255;
    const int edge = hw_x < 0 ? 0 : 239;
    const bool vertical = top <= bottom ? y >= top && y < bottom : y >= top || y < bottom;
    const bool horizontal = left <= right ? edge >= left && edge < right : edge >= left || edge < right;
    *out_mask = vertical && horizontal ? inside : outside;
    return 1;
}

void field_frame_policy(const gbarecomp::ExtendedViewFrameInfo*) {
    // Re-evaluate after every frame boundary, including state loads/rollback.
    // A restored frame counter must not reuse a previous scene's map pointers.
    g_frame = {};
    gba::GbaBus* bus = gbarecomp::active_bus();
    const FieldFrame& frame = current_frame(bus);
    gba::g_ws_pillarbox = (frame.field || (bus && equipment_registers(bus->io().raw()))) ? 0 : 1;
    gba::g_ws_pillarbox_left = 0;
    gba::g_ws_pillarbox_right = 0;
}

}  // namespace

void enable_equipment_widescreen(bool enabled) {
    g_equipment_widescreen = enabled;
    g_frame = {};
}

void configure_run_options(gbarecomp::RunOptions& opts) {
    // Adaptive widescreen follows the window/fullscreen aspect: 16:9 at 160
    // lines is ~284 px; wider displays clamp to 480 while preserving the
    // authored field data and the game's native vertical resolution.
    opts.mod_owns_adaptive_view = true;
    opts.max_view_width = 480;
    opts.max_resize_view_width = 480;
    opts.resize_driven_view = true;
    opts.extended_view_init = &install_extended_view;
    opts.extended_view_frame = &field_frame_policy;
    opts.launcher_expose_widescreen = false;
    opts.launcher_expose_adaptive_view = false;
}

gbarecomp::GbaNetplayViewPolicy netplay_view_policy() {
    gbarecomp::GbaNetplayViewPolicy policy;
    policy.supported = true;
    policy.adaptive_supported = true;
    policy.init = &install_extended_view;
    policy.frame = &field_frame_policy;
    policy.reset = &reset_extended_view;
    policy.max_width = 480;
    return policy;
}

void install_extended_view(std::uint32_t, std::uint32_t) {
    g_frame = {};
    gba::g_ws_tilemap_provider = &field_tilemap_provider;
    gba::g_ws_bg_x_provider = &hud_bg_x_provider;
    gba::g_ws_bg_x_provider_layers = (1u << 0) | (1u << 3);
    gba::g_ws_obj_attr_x_provider = &hud_obj_x_provider;
    gba::g_ws_margin_window_provider = &transition_window_provider;
    gba::g_ws_authored_margin_layers = 1;
    gba::g_ws_pillarbox = 1;
    gba::g_ws_pillarbox_left = 0;
    gba::g_ws_pillarbox_right = 0;
}

void reset_extended_view() {
    g_frame = {};
}

}  // namespace ss2
