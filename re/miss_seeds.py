"""Turn self-heal misses into reviewed [[extra_func]] candidates.

usage: miss_seeds.py <rom> <generated_dir> <log>...
Reads cart Thumb miss PCs from runtime logs ("compile FAILED for 0x...",
"bridged 0x...", "SELF-HEAL bridge for 0x..."), finds the enclosing function
start (a push {..,lr} at or before the miss), skips starts that are already
generated, and prints TOML for a human to review before merging.
"""
import glob
import re
import struct
import sys

rom = open(sys.argv[1], "rb").read()
gen_dir = sys.argv[2]
logs = sys.argv[3:]


def h(a):
    return struct.unpack_from("<H", rom, a - 0x08000000)[0]


def is_push_lr(x):
    return (x & 0xFF00) == 0xB500


def is_return(x):
    # bx rN, pop {..,pc}
    return (x & 0xFF87) == 0x4700 or (x & 0xFF00) == 0xBD00


misses = {}
pat = re.compile(r"(?:compile FAILED for|bridged|SELF-HEAL bridge for) 0x(08[0-9A-Fa-f]{6})(?: \(thumb\))?")
for path in logs:
    for line in open(path, encoding="utf-8", errors="ignore"):
        for m in pat.finditer(line):
            misses[int(m.group(1), 16)] = line.strip()[:90]

generated = set()
for f in glob.glob(gen_dir + "/recompiled_*.cpp"):
    for m in re.finditer(r"/\* 0x(08[0-9A-F]{6})  mode=", open(f, encoding="utf-8", errors="ignore").read()):
        generated.add(int(m.group(1), 16))

words = struct.unpack("<%dI" % (len(rom) // 4), rom[: len(rom) // 4 * 4])
refs = {}
for i, v in enumerate(words):
    if 0x08000000 <= v < 0x09000000 and v & 1:
        refs.setdefault(v & ~1, []).append(0x08000000 + i * 4)

starts = {}
for pc in sorted(misses):
    start = None
    a = pc
    while a > pc - 0x1000:
        if is_push_lr(h(a)):
            start = a
            break
        a -= 2
    if start is None:
        print("# 0x%08X: no prologue found, review by hand" % pc)
        continue
    starts.setdefault(start, []).append(pc)

for start, pcs in sorted(starts.items()):
    state = "already generated" if start in generated else "NEW"
    r = ", ".join("0x%08X" % x for x in refs.get(start, [])[:4]) or "none"
    print("# %s: start 0x%08X covers %s; pointer refs: %s" % (
        state, start, ", ".join("0x%08X" % p for p in pcs), r))
    if start not in generated:
        print("[[extra_func]]\naddr = 0x%08X\nmode = \"thumb\"\nnote = \"contains runtime misses %s\"\n" % (
            start, ", ".join("0x%08X" % p for p in pcs)))
