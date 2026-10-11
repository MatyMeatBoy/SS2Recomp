#pragma once

#include "runtime_arm.h"
#include <cstdio>
#include <cstdlib>

namespace ss2 {

// Nintendo's agb_flash library (FLASH512_V131) copies these helpers into
// executable RAM and calls the copy in Thumb mode. Most copies use IWRAM
// stack buffers; ReadFlash1 also appears at a fixed EWRAM thunk (0x02001688).
// Match the entire live copy, including any inline literal pool, before
// dispatching the same instruction offset in the ROM's compiled body. The
// copy bounds come from the library's own copy loops:
//
//   0x080B87C8 SetReadFlash1      [0x080B87C4, 0x080B87C8)  pool 0x080B87E8
//   0x080B882C ReadFlash          [0x080B8808, 0x080B882C)  pool 0x080B8884
//   0x080B88F8 VerifyFlashSector  [0x080B88C8, 0x080B88F8)  pool 0x080B894C
//   0x080B8D18 ProgramFlashSector [0x080B8E3C, 0x080B8E60)  pool 0x080B8D5C
//
// All branches stay inside the copied span, PC-relative loads refer to copied
// literals, and no instruction exposes its code address as data.
inline int flash_stack_dispatch(uint32_t pc, int thumb) {
    if (!thumb || (pc & 1u)) return 0;

    uint32_t ram_begin = 0;
    uint32_t ram_end = 0;
    if (pc >= 0x02000000u && pc < 0x02040000u) {
        ram_begin = 0x02000000u;
        ram_end = 0x02040000u;
    } else if (pc >= 0x03000000u && pc < 0x03008000u) {
        ram_begin = 0x03000000u;
        ram_end = 0x03008000u;
    } else {
        return 0;
    }

    struct Copy { uint32_t source; uint32_t size; };
    static constexpr Copy copies[] = {
        {0x080B88C8u, 0x30u}, // VerifyFlashSector_Core -> VerifyFlashSector
        {0x080B8808u, 0x24u}, // ReadFlash_Core -> ReadFlash
        {0x080B8E3Cu, 0x24u}, // VerifyFlashCoreFF -> ProgramFlashSector
        {0x080B87C4u, 0x04u}, // ReadFlash1 -> SetReadFlash1
    };
    for (const Copy& copy : copies) {
        uint32_t first = pc - (copy.size - 2u);
        if (first < ram_begin) first = ram_begin;
        first = (first + 3u) & ~3u;
        for (uint32_t start = first; start <= pc && start + copy.size <= ram_end;
             start += 4u) {
            uint32_t offset = 0;
            for (; offset < copy.size; offset += 4u) {
                if (bus_read_u32(start + offset) != bus_read_u32(copy.source + offset)) break;
            }
            if (offset != copy.size) continue;
            runtime_dispatch(copy.source + (pc - start));
            return 1;
        }
    }
    if (std::getenv("GBARECOMP_SS2_RAM_TRACE")) {
        std::fprintf(stderr, "ss2_ram_miss pc=%08X bytes=", pc);
        for (uint32_t i = 0; i < 16; i += 4)
            std::fprintf(stderr, "%08X", bus_read_u32((pc & ~3u) + i));
        std::fprintf(stderr, "\n");
    }
    return 0;
}

}  // namespace ss2
