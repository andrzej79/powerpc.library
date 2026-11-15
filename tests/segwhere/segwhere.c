/* segwhere - where powerpc.library's patches put a 68k program (17.17 test).
 *
 * Prints the TF_PPC bit of its own process and the addresses of its first and second
 * code hunks, a data and a BSS object, a stack variable and an AllocMem block, with
 * "card" after each address that is in PPC memory (from powerpc.library's own memory
 * list, MEMF_PPC). It holds the string "powerpc.library" in a data hunk, which made
 * 17.16's loader keep it in PPC memory.
 *
 *   segwhere           as loaded
 *   segwhere OPEN      also after OpenLibrary("powerpc.library") (17.17: no tag for a
 *                      68k-only program)
 *
 * Build: vc +aos68k -c99 -O1 segwhere.c -o segwhere (two code sections: vasm makes two
 * code hunks).
 */
#include <exec/exec.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>

#define TF_PPC   (1 << 2)
#define MEMF_PPC (1L << 13)

const char libName[] = "powerpc.library";   /* data hunk: the string 17.16 looked for */
static char bssBlock[64];
static int dataWord = 0x12345678;

static const char* where(APTR p)
{
    struct MemHeader* mh;
    const char* r = "amiga";

    Forbid();
    for (mh = (struct MemHeader*)SysBase->MemList.lh_Head; mh->mh_Node.ln_Succ; mh = (struct MemHeader*)mh->mh_Node.ln_Succ) {
        if ((p >= mh->mh_Lower) && (p < mh->mh_Upper)) {
            r = (mh->mh_Attributes & MEMF_PPC) ? "CARD" : ((mh->mh_Attributes & MEMF_CHIP) ? "chip" : "fast");
            break;
        }
    }
    Permit();
    return r;
}

static void report(const char* when, APTR secondHunk)
{
    struct Task* me = FindTask(NULL);
    APTR block = AllocMem(4096, MEMF_PUBLIC);
    int local = 0;

    Printf("%s: TF_PPC=%ld\n", (LONG)when, (LONG)((me->tc_Flags & TF_PPC) ? 1 : 0));
    Printf("  code hunk 1  %08lx %s\n", (LONG)report, (LONG)where((APTR)report));
    Printf("  code hunk 2  %08lx %s\n", (LONG)secondHunk, (LONG)where(secondHunk));
    Printf("  data         %08lx %s\n", (LONG)&dataWord, (LONG)where(&dataWord));
    Printf("  bss          %08lx %s\n", (LONG)bssBlock, (LONG)where(bssBlock));
    Printf("  stack        %08lx %s\n", (LONG)&local, (LONG)where(&local));
    Printf("  task         %08lx %s\n", (LONG)me, (LONG)where(me));
    if (block) {
        Printf("  AllocMem     %08lx %s\n", (LONG)block, (LONG)where(block));
        FreeMem(block, 4096);
    }
}

__section("CODE2") APTR secondHunkFunc(void)
{
    return (APTR)secondHunkFunc;
}

int main(int argc, char** argv)
{
    APTR second = secondHunkFunc();

    report("loaded", second);
    if ((argc > 1) && ((argv[1][0] | 0x20) == 'o')) {
        struct Library* ppc = OpenLibrary((STRPTR)libName, 0);

        if (ppc) {
            report("powerpc.library open", second);
            CloseLibrary(ppc);
        } else {
            Printf("no %s\n", (LONG)libName);
        }
    }
    bssBlock[0] = (char)dataWord;
    return 0;
}
