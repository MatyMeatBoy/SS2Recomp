"""Find spans of a RAM dump that are verbatim copies of ROM bytes.

usage: find_copies.py <rom.gba> <ram.bin> <ram_base_hex> [min_len]

Prints runtime_start / source_start / size candidates for [[code_copy]] entries.
Copies are found by indexing every 4-byte-aligned 16-byte ROM window and then
extending matches in both directions.
"""
import sys

def main():
    rom = open(sys.argv[1], "rb").read()
    ram = open(sys.argv[2], "rb").read()
    base = int(sys.argv[3], 16)
    min_len = int(sys.argv[4]) if len(sys.argv) > 4 else 64
    K = 16
    index = {}
    for off in range(0, len(rom) - K, 4):
        key = rom[off:off + K]
        if key.count(key[0]) == K:  # skip runs of a single byte (padding)
            continue
        index.setdefault(key, off)
    i = 0
    found = []
    while i + K <= len(ram):
        key = ram[i:i + K]
        src = index.get(key)
        if src is None or key.count(key[0]) == K:
            i += 4
            continue
        start, s = i, src
        while start > 0 and s > 0 and ram[start - 1] == rom[s - 1]:
            start -= 1
            s -= 1
        end, e = i + K, src + K
        while end < len(ram) and e < len(rom) and ram[end] == rom[e]:
            end += 1
            e += 1
        if end - start >= min_len:
            found.append((base + start, 0x08000000 + s, end - start))
        i = max(end, i + 4)
        i = (i + 3) & ~3
    for rt, src, size in found:
        print(f"runtime_start=0x{rt:08X} source_start=0x{src:08X} size=0x{size:X} ({size} bytes)")

if __name__ == "__main__":
    main()
