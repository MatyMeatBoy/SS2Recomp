"""Minimal Thumb disassembler for switch/dispatch inspection.

usage: thumb_dis.py <rom> <start_hex> <end_hex>
Decodes the common ARMv4T Thumb formats; unknown halfwords print as .hword.
"""
import struct
import sys

rom = open(sys.argv[1], "rb").read()
start, end = int(sys.argv[2], 16), int(sys.argv[3], 16)
R = lambda n: "r%d" % n if n < 13 else ("sp", "lr", "pc")[n - 13]
COND = ["eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc", "hi", "ls", "ge", "lt", "gt", "le"]


def h(a):
    return struct.unpack_from("<H", rom, a - 0x08000000)[0]


def w(a):
    return struct.unpack_from("<I", rom, a - 0x08000000)[0]


a = start
while a < end:
    x = h(a)
    s = ".hword 0x%04X" % x
    if x >> 13 == 0 and (x >> 11) & 3 != 3:
        op = ["lsl", "lsr", "asr"][(x >> 11) & 3]
        s = "%s %s,%s,#%d" % (op, R(x & 7), R((x >> 3) & 7), (x >> 6) & 31)
    elif x >> 11 == 3:
        op = "sub" if x & 0x200 else "add"
        rn = ("#%d" % ((x >> 6) & 7)) if x & 0x400 else R((x >> 6) & 7)
        s = "%s %s,%s,%s" % (op, R(x & 7), R((x >> 3) & 7), rn)
    elif x >> 13 == 1:
        op = ["mov", "cmp", "add", "sub"][(x >> 11) & 3]
        s = "%s %s,#0x%X" % (op, R((x >> 8) & 7), x & 0xFF)
    elif x >> 10 == 0x10:
        ops = ["and", "eor", "lsl", "lsr", "asr", "adc", "sbc", "ror", "tst", "neg", "cmp", "cmn", "orr", "mul", "bic", "mvn"]
        s = "%s %s,%s" % (ops[(x >> 6) & 15], R(x & 7), R((x >> 3) & 7))
    elif x >> 10 == 0x11:
        op = (x >> 8) & 3
        rd = (x & 7) | ((x >> 4) & 8)
        rs = (x >> 3) & 15
        s = ["add %s,%s", "cmp %s,%s", "mov %s,%s", "bx %s%s"][op] % (R(rd) if op < 3 else "", R(rs))
    elif x >> 11 == 9:
        lit = ((a + 4) & ~3) + (x & 0xFF) * 4
        s = "ldr %s,[pc,#0x%X] ; =0x%08X" % (R((x >> 8) & 7), (x & 0xFF) * 4, w(lit))
    elif x >> 12 == 5:
        ops = ["str", "strh", "strb", "ldsb", "ldr", "ldrh", "ldrb", "ldsh"]
        s = "%s %s,[%s,%s]" % (ops[(x >> 9) & 7], R(x & 7), R((x >> 3) & 7), R((x >> 6) & 7))
    elif x >> 13 == 3:
        b = (x >> 12) & 1
        l = (x >> 11) & 1
        off = ((x >> 6) & 31) * (1 if b else 4)
        s = "%s%s %s,[%s,#0x%X]" % ("ldr" if l else "str", "b" if b else "", R(x & 7), R((x >> 3) & 7), off)
    elif x >> 12 == 8:
        s = "%sh %s,[%s,#0x%X]" % ("ldr" if x & 0x800 else "str", R(x & 7), R((x >> 3) & 7), ((x >> 6) & 31) * 2)
    elif x >> 12 == 9:
        s = "%s %s,[sp,#0x%X]" % ("ldr" if x & 0x800 else "str", R((x >> 8) & 7), (x & 0xFF) * 4)
    elif x >> 12 == 10:
        s = "add %s,%s,#0x%X" % (R((x >> 8) & 7), "sp" if x & 0x800 else "pc", (x & 0xFF) * 4)
    elif x >> 8 == 0xB0:
        s = "%s sp,#0x%X" % ("sub" if x & 0x80 else "add", (x & 0x7F) * 4)
    elif x >> 12 == 0xB and (x >> 9) & 3 == 2:
        regs = [R(i) for i in range(8) if x & (1 << i)]
        if x & 0x100:
            regs.append("pc" if x & 0x800 else "lr")
        s = "%s {%s}" % ("pop" if x & 0x800 else "push", ",".join(regs))
    elif x >> 12 == 0xC:
        regs = [R(i) for i in range(8) if x & (1 << i)]
        s = "%s %s!,{%s}" % ("ldmia" if x & 0x800 else "stmia", R((x >> 8) & 7), ",".join(regs))
    elif x >> 12 == 0xD and (x >> 8) & 15 < 14:
        off = x & 0xFF
        off = off - 256 if off & 0x80 else off
        s = "b%s 0x%08X" % (COND[(x >> 8) & 15], a + 4 + off * 2)
    elif x >> 8 == 0xDF:
        s = "swi 0x%X" % (x & 0xFF)
    elif x >> 11 == 0x1C:
        off = x & 0x7FF
        off = off - 2048 if off & 0x400 else off
        s = "b 0x%08X" % (a + 4 + off * 2)
    elif x >> 11 == 0x1E and a + 2 < end + 4 and h(a + 2) >> 11 == 0x1F:
        hi = x & 0x7FF
        hi = hi - 2048 if hi & 0x400 else hi
        lo = h(a + 2) & 0x7FF
        print("%08X  %04X %04X  bl 0x%08X" % (a, x, h(a + 2), a + 4 + (hi << 12) + (lo << 1)))
        a += 4
        continue
    print("%08X  %04X       %s" % (a, x, s))
    a += 2
