#include "ss2_extended_view.h"
#include "gba_bus.h"
#include "gba_ppu.h"
#include "runtime_bus_bridge.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <filesystem>
extern "C" unsigned g_ws_extra_left;
extern "C" unsigned g_ws_extra_right;

std::vector<std::uint8_t> read(const char* path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error(path);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
void load(const char* path, std::uint8_t* destination, std::size_t size) {
    auto data = read(path);
    if (data.size() != size) throw std::runtime_error("fixture size mismatch");
    std::copy(data.begin(), data.end(), destination);
}
void put(std::uint8_t* p, unsigned off, unsigned value) {
    p[off] = value; p[off+1] = value >> 8;
}
int main(int argc, char** argv) {
    try {
        auto rom = read("roms/shining_soul_2_europe.gba");
        gba::GbaBus bus;
        gba::GbaPpu ppu;
        bus.set_rom(rom.data(), rom.size());
        load("re/run/ws4_iwram.bin",bus.iwram_ptr(),32768);
        load("re/run/ws4_ewram.bin",bus.ewram_ptr(),262144);
        load("re/run/ws4_vram.bin",bus.vram_ptr(),98304);
        auto io = read("re/run/ws4_io.bin");
        for (unsigned i=0;i<io.size();i+=2)
            bus.io().write16(i,io[i] | (unsigned(io[i+1])<<8));
        gbarecomp::set_active_bus(&bus);
        ss2::install_extended_view(22,22);
        ppu.set_view_margins(22,22,0,0);
        auto policy=ss2::netplay_view_policy();
        policy.frame(nullptr);
        for (int x : {0,239,255,256,260,304,448,511}) {
            int decoded=0;
            if (!gba::g_ws_obj_attr_x_provider(0,64,x,2<<10,&decoded) ||
                decoded != (x >= 384 ? x-512 : x))
                throw std::runtime_error("world sprite wrapped at right edge");
        }
        // Real PPU composition, with one red 8x8 world OBJ straddling X=256.
        for (unsigned i=0;i<128;++i) put(bus.oam_ptr(),i*8,0x200);
        put(bus.oam_ptr(),0,64); put(bus.oam_ptr(),2,256); put(bus.oam_ptr(),4,0);
        std::fill_n(bus.vram_ptr()+0x10000,32,0x11);
        put(bus.pal_ptr(),0x202,31);
        std::vector<std::uint8_t> image(ppu.render_bytes());
        ppu.render(image.data(),0x1f40,bus.io().raw(),bus.vram_ptr(),bus.oam_ptr(),bus.pal_ptr());
        auto pixel=(64*284+278)*3;
        if (image[pixel] < 240 || image[pixel+1] || image[pixel+2])
            throw std::runtime_error("right-margin OBJ not rendered");
        auto mask_io=io;
        put(mask_io.data(),0,0x3f40);put(mask_io.data(),0x40,0xf0);
        put(mask_io.data(),0x44,0xa0);put(mask_io.data(),0x48,0);put(mask_io.data(),0x4a,0x3f3f);
        ppu.render(image.data(),0x3f40,mask_io.data(),bus.vram_ptr(),bus.oam_ptr(),bus.pal_ptr());
        if (std::any_of(image.begin(),image.end(),[](auto x){return x!=0;}))
            throw std::runtime_error("closed transition leaked BG or OBJ pixels");
        std::cout << "PASS: OBJ X=256, negative wrap, closed transition BG+OBJ\n";
        if (argc == 2) {
            const std::filesystem::path fixture(argv[1]);
            auto menu_io = read((fixture / "0240-io.bin").string().c_str());
            load((fixture / "0240-vram.bin").string().c_str(),bus.vram_ptr(),98304);
            load((fixture / "0240-pal.bin").string().c_str(),bus.pal_ptr(),1024);
            for (unsigned i=0;i<menu_io.size();i+=2)
                bus.io().write16(i,menu_io[i] | (unsigned(menu_io[i+1])<<8));
            ss2::enable_equipment_widescreen(true);
            g_ws_extra_left = g_ws_extra_right = 22;
            policy.frame(nullptr);
            // Draw only the decorative plane to compare reflection against its
            // unobstructed native pixels, independent of the inventory UI.
            ppu.render(image.data(),0x0840,bus.io().raw(),bus.vram_ptr(),bus.oam_ptr(),bus.pal_ptr());
            for (int y=0;y<160;++y) for (int x=0;x<22;++x)
                for (int c=0;c<3;++c) {
                    if (image[(y*284+x)*3+c] != image[(y*284+43-x)*3+c] ||
                        image[(y*284+262+x)*3+c] != image[(y*284+261-x)*3+c])
                        throw std::runtime_error("inventory background reflection mismatch");
                }
            auto decorated=image;
            ppu.render(image.data(),0x1d40,bus.io().raw(),bus.vram_ptr(),bus.oam_ptr(),bus.pal_ptr());
            for (int y=0;y<160;++y) for (int x=0;x<284;++x) {
                if (x>=22 && x<262) continue;
                for (int c=0;c<3;++c)
                    if (image[(y*284+x)*3+c] != decorated[(y*284+x)*3+c])
                        throw std::runtime_error("inventory UI leaked into mirrored margins");
            }
            ss2::enable_equipment_widescreen(false);
            policy.frame(nullptr);
            if (!gba::g_ws_pillarbox)
                throw std::runtime_error("disabled inventory mod retained widescreen");
            std::cout << "PASS: mirrored inventory pixels, no margin UI leakage, disable reset\n";
        }
        gbarecomp::set_active_bus(nullptr);
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n';return 1; }
}
