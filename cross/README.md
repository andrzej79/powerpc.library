# Cross-building powerpc.library on macOS

Builds `powerpc.library` from this repository's sources (the parent directory) with vbcc, on macOS
(Apple silicon tested). The Amiga-hosted `makefile` at the top is the original author's and is
left as it is.

```sh
cd cross
make            # builds the private PPC tools if needed, then build/powerpc.library
make verify     # compares the result with SHIPPED, hunk by hunk
make clean      # removes build/
make distclean  # also removes toolchain/
```

17.18 built here is byte-identical to the one tested on the A3000 (2026-11-16).

Defaults can be overridden, for example `make NDK=/path/to/NDK3.2R4`:

| Variable | Default | Meaning |
|---|---|---|
| `VBCC` | `/opt/amiga_sdk/vbcc` | installed vbcc: `vbccm68k`, `vasmm68k_mot`, `vlink`, the m68k and ppc-warpos targets |
| `VBCC_SRC` | `/opt/amiga_sdk/download/vbcc-2022-11-01` | vbcc and vasm sources, for the private PPC tools |
| `NDK` | `/opt/amiga_sdk/NDK3.2R4` | Amiga NDK |
| `WOS_C_INC` | `$(VBCC)/targets/ppc-warpos/include` | the WarpOS C headers |
| `SRC` | `..` | the library's sources |
| `SHIPPED` | `../../a3000/LIBS/PowerPC/powerpc.library` | what `make verify` compares with |

## Rules the build depends on

- **Link order.** `LibInit` finds the PPC parts by walking the seglist: setup, kernel, functions.
  `main()` in `libinit.c` must come first.
- **No absolute references in PPC code.** Those hunks are copied to the card and never relocated.
  They reach each other through the jump table at `PowerPCBase`.
- **`vlink -bamigahunk -mrel`.** vasm emits `HUNK_PPC_CODE` and `-bamigahunk` turns it into plain
  `HUNK_CODE`. `-mrel` merges sections with PC-relative references.
- **`-O=1023` plus `vscppc`** is what `vc -O2` means. `-O=2` doubles the code size.

## Changes needed for this compiler

- **`0b` literals.** vasm 1.9a reads `0b1110` as a reference to local label 0, so `setupppc.s` is
  rewritten in hex into `build/setupppc_hex.s`. The source file is not changed.
- **Jump tables.** vbcc 0.9h compiles long dense `switch` statements to tables of absolute
  addresses, which breaks the rule above (link error: undefined `_SDA_BASE_`). The private
  `toolchain/vbcc` is built with a larger `JUMP_TABLE_LENGTH`; `/opt` is not modified.

## Debug build

`DEBUG` has to be defined on both the 68k and PPC sides. This builds into `build_debug/`:

```sh
make B=build_debug \
  'M68KCFLAGS=-quiet -cpu=68030 -sc -cpp-comments -c99 -O=1023 -speed -DDEBUG' \
  'PPCCFLAGS=-quiet -poweropen -sc -no-multiple-ccs -madd -c99 -O=1023 -speed -DDEBUG'
```

Messages go to the serial port through exec's `RawPutChar`. `DEBUG` also stops a program that
reads outside exec's memory list from the PPC (other than address 4) with a crash report.

## Headers

`include/` was recreated for this build; it is not the original header set, whose source is
unknown. Only what the library uses is defined, and unused field names are made up.

| File | Based on |
|---|---|
| `libraries/mediatorpci.h`, `proto/mediatorpci.h` | 17.13b's `pci.i` and `pci_lib.fd`, corrected against the shipped binary (`_LVOPCIFreeMem` is -156, not -154) |
| `libraries/prometheus.h`, `proto/prometheus.h` | P96 pragmas for offsets; tag base `0x6EDA0000` and the `Prm_*PCIAddressSpace` offsets from the shipped binary |
| `powerpc/powerpc.i` | offsets of `struct PPCArgs` |
| `clib/powerpc_protos.h` | empty on purpose |

`third_party/warpos/powerpc/` is `src/ppc/include/powerpc` from
[michalsc/Emu68](https://github.com/michalsc/Emu68) (MPL-2.0). The older headers in Aminet's
EGCS-WarpUP-Ad do not work: they lack `tp_Msgport` and `tp_TaskPools`.

Mediator support compiles but has not been tested (the machine this was built for has a FireBird).
