# Loader tests (17.17)

Three small 68k tools for checking where the LoadSeg patch puts a program, built with
vbcc (`vc +aos68k -c99 -O1 <file>.c -o <file> -lamiga`):

| Tool | What it shows |
|---|---|
| `segwhere [OPEN]` | Its own TF_PPC bit and where its two code hunks, data, BSS, stack, task and an `AllocMem` block are (fast, chip or CARD); `OPEN` repeats it after `OpenLibrary("powerpc.library")`. It holds the string `powerpc.library`, which made 17.16 keep it in PPC memory. |
| `ldwhere <file> [RUN]` | `LoadSeg()` of any file through the LVO: where each segment landed and the caller's TF_PPC before, after and after `UnLoadSeg()`. |
| `tasklist` | Every task with its TF_PPC bit and where its structure and stack are. |
| `ppcsys [ASYNC] [SYSSPEED] [NOPPC\|NOLIB]` | In its own CLI process: starts a program with `SystemTags`, `RunPPC`s a two-instruction PPC stub, starts it again; logs to `RAM:ppcsys.log`. `SYSSPEED` uses SysSpeed's tags for ADPro (`SYS_Asynch`, `NP_StackSize` 40000, no `SYS_Input`/`SYS_Output`). |

Results on an A3000 with a Harrier PPC7410 (2026-11-15), 17.16 against 17.17:

| File | 17.16 | 17.17 |
|---|---|---|
| `segwhere` (68k, holds the string) | 1 hunk fast, 6 CARD, caller left tagged | 7 fast, caller untouched |
| SysSpeed 2.6 | 68k code fast, PPC test hunk CARD, caller left tagged | the same, caller untouched |
| WarpQuake2PPC.engine (vbcc EHF, PPC code in HUNK_CODE) | 1 fast, 8 CARD, caller left tagged | the same, caller untouched |
| minigl_ppc.dll (Elf2Exe2, PPC code in HUNK_DATA) | 1 fast, 12 CARD, caller left tagged | the same, caller untouched |

Quake II GL `timedemo demo1` at 1280x720: 17.16 19.74-19.80 ms, 17.17 19.73-19.77 ms.

SysSpeed's "ADPro after the PPC test" hang from a Shell (2026-11-15) is not the library's: `ppcsys SYSSPEED NOLIB`
in a Shell, with powerpc.library never started, leaves the asynchronous children stuck (wait on bit 31) or kills them
with exec alerts ("Freeing memory that is already free", "Corrupt memory list detected in FreeMem"). SysSpeed starts
ADPro with `SYS_Asynch` and no streams, so the child gets the Shell's own console handles and closes them when it ends.
From Workbench `Input()`/`Output()` are 0 and it works.
