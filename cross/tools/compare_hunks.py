#!/usr/bin/env python3
"""Compare a rebuilt powerpc.library with the shipped one.

Exits non-zero if the rebuild breaks a rule the library depends on. A difference
in code bytes alone is not a failure: the compiler version is not the original.
"""
import collections
import difflib
import struct
import sys

CODE, DATA, BSS, RELOC32, DREL32, RELOC32SHORT, END = 0x3E9, 0x3EA, 0x3EB, 0x3EC, 0x3F7, 0x3FC, 0x3F2
NAMES = ["68k code", "PPC setup", "PPC kernel", "PPC functions", "68k data", "68k BSS"]


def load(path):
    """Return a list of hunks as dicts: type, data, size, relocs {target: count}."""
    d = open(path, "rb").read()
    p = 0

    def long():
        nonlocal p
        v = struct.unpack(">I", d[p : p + 4])[0]
        p += 4
        return v

    def word():
        nonlocal p
        v = struct.unpack(">H", d[p : p + 2])[0]
        p += 2
        return v

    if long() != 0x3F3:
        sys.exit(f"{path}: not an AmigaOS load file")
    while long():
        pass
    long()
    first, last = long(), long()
    p += 4 * (last - first + 1)
    hunks = []
    while p < len(d):
        t = long() & 0x3FFFFFFF
        if t in (CODE, DATA):
            n = long() * 4
            hunks.append(dict(type=t, data=d[p : p + n], size=n, relocs=collections.Counter()))
            p += n
        elif t == BSS:
            hunks.append(dict(type=t, data=b"", size=long() * 4, relocs=collections.Counter()))
        elif t == RELOC32:
            while n := long():
                target = long()
                hunks[-1]["relocs"][target] += n
                p += 4 * n
        elif t in (DREL32, RELOC32SHORT):
            while n := word():
                target = word()
                hunks[-1]["relocs"][target] += n
                p += 2 * n
            if p % 4:
                p += 2
        elif t == END:
            pass
        else:
            sys.exit(f"{path}: unexpected hunk type {t:#x} at offset {p - 4:#x}")
    return hunks


def jsr_counts(code):
    return collections.Counter(code[i + 2 : i + 4] for i in range(0, len(code) - 3, 2) if code[i : i + 2] == b"\x4e\xae")


def main():
    if len(sys.argv) != 3:
        sys.exit(f"usage: {sys.argv[0]} shipped.library rebuilt.library")
    shipped, built = load(sys.argv[1]), load(sys.argv[2])
    failures = []

    def check(ok, text):
        print(("  ok    " if ok else "  FAIL  ") + text)
        if not ok:
            failures.append(text)

    print(f"{'hunk':<15}{'shipped':>10}{'rebuilt':>10}{'diff':>7}")
    for i, name in enumerate(NAMES[: min(len(shipped), len(built))]):
        a, b = shipped[i]["size"], built[i]["size"]
        print(f"{name:<15}{a:>#10x}{b:>#10x}{b - a:>+7d}")
    print()

    check(len(built) == 6, f"six hunks ({len(built)})")
    if len(built) != 6 or len(shipped) != 6:
        sys.exit(1)
    for i in (1, 2, 3):
        check(not built[i]["relocs"], f"{NAMES[i]} has no relocations")
    check(built[1]["data"][:4] == b"\x48\x00\x00\x45", "setup starts with bl .SkipCom")
    check(built[2]["data"][:4] == b"\x48\x00\x0c\x18", "kernel starts with b ExcITLBMiss")
    check(built[0]["data"][:4] == b"\x70\xff\x4e\x75", "68k code starts with main(), return -1")
    check(built[0]["relocs"][3] == 98, "98 relocations from 68k code into PPC functions")
    check(built[0]["relocs"] == shipped[0]["relocs"], "68k relocation counts match")
    check(built[4]["relocs"] == shipped[4]["relocs"], "data relocation counts match")
    check(built[0]["size"] == shipped[0]["size"], "68k code size matches")
    check(built[4]["data"] == shipped[4]["data"], "68k data is byte-identical")
    check(built[5]["size"] == shipped[5]["size"], "BSS size matches")
    check(jsr_counts(built[0]["data"]) == jsr_counts(shipped[0]["data"]), "library call offsets match in count")

    wa = [shipped[0]["data"][i : i + 2] for i in range(0, shipped[0]["size"], 2)]
    wb = [built[0]["data"][i : i + 2] for i in range(0, built[0]["size"], 2)]
    match = sum(m.size for m in difflib.SequenceMatcher(None, wa, wb, autojunk=False).get_matching_blocks())
    print(f"\n68k code: {match} of {len(wa)} words line up in sequence ({100 * match / len(wa):.1f}%)")

    print("\nThis is a static check. It does not show that the library boots.")
    if failures:
        print(f"{len(failures)} check(s) failed.")
        sys.exit(1)


if __name__ == "__main__":
    main()
