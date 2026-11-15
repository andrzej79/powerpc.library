/* ldwhere - LoadSeg() a file through dos.library's LVO (and so through powerpc.library's
 * patch), print where each segment landed, and the caller's TF_PPC before and after.
 *
 *   ldwhere <file> [RUN]    RUN also runs it with RunCommand (no arguments) before unloading
 *
 * Build: vc +aos68k -c99 -O1 ldwhere.c -o ldwhere -lamiga
 */
#include <exec/exec.h>
#include <exec/memory.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>

#define TF_PPC   (1 << 2)
#define MEMF_PPC (1L << 13)

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

static LONG flag(void)
{
    return (FindTask(NULL)->tc_Flags & TF_PPC) ? 1 : 0;
}

int main(int argc, char** argv)
{
    BPTR seg, s;
    int n = 0;

    if (argc < 2) {
        Printf("ldwhere <file> [RUN]\n");
        return 5;
    }
    Printf("TF_PPC before LoadSeg: %ld\n", flag());
    if (!(seg = LoadSeg((STRPTR)argv[1]))) {
        Printf("LoadSeg failed (%ld)\n", IoErr());
        return 10;
    }
    Printf("TF_PPC after LoadSeg:  %ld\n", flag());
    for (s = seg; s; s = *(BPTR*)BADDR(s)) {
        ULONG* p = (ULONG*)BADDR(s);
        Printf("  segment %ld  %08lx  %6ld bytes  %s\n", (LONG)n++, (LONG)(p + 1), (LONG)(p[-1] - 8), (LONG)where(p + 1));
    }
    if ((argc > 2) && ((argv[2][0] | 0x20) == 'r')) {
        LONG rc = RunCommand(seg, 65536, (STRPTR)"\n", 1);
        Printf("RunCommand returned %ld, TF_PPC now %ld\n", rc, flag());
    }
    UnLoadSeg(seg);
    Printf("TF_PPC after UnLoadSeg: %ld\n", flag());
    return 0;
}
