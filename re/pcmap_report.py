"""Summarize executed RAM PCs recorded by `pgba_test ... pcmap`.

usage: pcmap_report.py <file.pcmap> [copies.txt]
Groups executed halfwords into ranges (gap tolerance 0x40 bytes) and labels
each range Thumb/ARM; optionally marks which [[code_copy]] candidate covers it.
"""
import re
import sys

data = open(sys.argv[1], "rb").read()
copies = []
if len(sys.argv) > 2:
    for line in open(sys.argv[2]):
        m = re.search(r"runtime_start=0x([0-9A-F]+) source_start=0x([0-9A-F]+) size=0x([0-9A-F]+)", line)
        if m:
            copies.append(tuple(int(x, 16) for x in m.groups()))

def addr(i):
    return 0x03000000 + i * 2 if i < 0x4000 else 0x02000000 + (i - 0x4000) * 2

hits = [(addr(i), v) for i, v in enumerate(data) if v]
ranges = []
for a, v in hits:
    if ranges and a - ranges[-1][1] <= 0x40 and (a >> 24) == (ranges[-1][0] >> 24):
        ranges[-1][1] = a
        ranges[-1][2] |= v
        ranges[-1][3] += 1
    else:
        ranges.append([a, a, v, 1])
for start, end, mode, n in ranges:
    kind = {1: "thumb", 2: "arm", 3: "arm+thumb"}[mode]
    cover = ""
    for rt, src, size in copies:
        if rt <= start and end < rt + size:
            cover = f"  <- copia de ROM 0x{src + (start - rt):08X} (bloque 0x{rt:08X}+0x{size:X})"
    print(f"0x{start:08X}..0x{end:08X} {kind:9s} {n:5d} muestras{cover}")
