/* ppcsys - a CLI process that runs PPC code, then starts programs (17.17 test).
 *
 * SysSpeed started from a Shell hangs or resets when it starts ADPro after its PPC
 * test; from Workbench it works. This does the same in its own CLI process: start a
 * program, RunPPC a two-instruction PPC stub, start the program again, printing the
 * process's signal state at each step.
 *
 *   ppcsys [ASYNC] [SYSSPEED] [NOPPC|NOLIB]   NOLIB: never opens powerpc.library;   ASYNC: SYS_Asynch with NIL: streams; SYSSPEED: SysSpeed's
 *                                       ADPro tags (SYS_Asynch, NP_StackSize 40000, no streams)
 *
 * Build: vc +aos68k -c99 -O1 ppcsys.c -o ppcsys -lamiga
 */
#include <exec/exec.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <powerpc/powerpc.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/powerpc.h>
#include <string.h>

struct Library* PowerPCBase;
static BPTR logFile;      /* RAM:ppcsys.log, a copy of every line (the screen may be out of reach) */

static void state(const char* when)
{
    struct Task* me = FindTask(NULL);

    Printf("%-22s SigAlloc %08lx SigWait %08lx SigRecvd %08lx flags %02lx\n", (LONG)when, me->tc_SigAlloc, me->tc_SigWait,
           me->tc_SigRecvd, (LONG)me->tc_Flags);
    Flush(Output());
    if (logFile) {
        FPrintf(logFile, "%s: SigAlloc %08lx flags %02lx\n", (LONG)when, me->tc_SigAlloc, (LONG)me->tc_Flags);
        Flush(logFile);
    }
}

static int sysspeed;

static void start(int async)
{
    LONG rc;

    if (sysspeed) {
        /* SysSpeed's own tags for ADPro: no streams given, asynchronous, NP_StackSize 40000 */
        rc = SystemTags((STRPTR) "C:Wait 1", SYS_Asynch, TRUE, NP_StackSize, 40000, TAG_DONE);
        Printf("  SystemTags async, SysSpeed's tags: %ld\n", rc);
        if (logFile) {
            FPrintf(logFile, "  SystemTags async, SysSpeed's tags: %ld\n", rc);
            Flush(logFile);
        }
        Delay(100);     /* let the child end (and close what it was given) before the next start */
    } else if (async) {
        BPTR in = Open((STRPTR) "NIL:", MODE_OLDFILE);
        BPTR out = Open((STRPTR) "NIL:", MODE_NEWFILE);

        rc = SystemTags((STRPTR) "C:Wait 1", SYS_Asynch, TRUE, SYS_Input, (ULONG)in, SYS_Output, (ULONG)out, TAG_DONE);
        Printf("  SystemTags async: %ld\n", rc);
    } else {
        rc = SystemTags((STRPTR) "C:Echo >NIL: hello", TAG_DONE);
        Printf("  SystemTags sync: %ld\n", rc);
    }
    Flush(Output());
}

int main(int argc, char** argv)
{
    int async = 0, noppc = 0, nolib = 0, i;
    struct PPCArgs args;
    ULONG* stub;

    logFile = Open((STRPTR) "RAM:ppcsys.log", MODE_NEWFILE);
    for (i = 1; i < argc; i++) {
        if ((argv[i][0] | 0x20) == 'a') async = 1;
        if ((argv[i][0] | 0x20) == 'n') noppc = 1;
        if ((argv[i][0] | 0x20) == 's') sysspeed = 1;
        if (((argv[i][0] | 0x20) == 'n') && ((argv[i][2] | 0x20) == 'l')) nolib = 1;
    }
    if (nolib) {
        /* powerpc.library never opened: no patches of it are in the system (run before InitPPC) */
        state("start, no library");
        start(async);
        state("after first start");
        start(async);
        state("after second start");
        start(async);
        state("after third start");
        Printf("done\n");
        if (logFile) {
            FPrintf(logFile, "done\n");
            Close(logFile);
        }
        return 0;
    }
    {
        char name[20];
        strcpy(name, "power");
        strcat(name, "pc.library");     /* built at run time: the string in the file made 17.16 keep it in PPC memory */
        if (!(PowerPCBase = OpenLibrary((STRPTR)name, 15))) {
            Printf("no %s\n", (LONG)name);
            return 10;
        }
    }
    state("start");
    start(async);
    state("after first start");
    if (!noppc) {
        if (!(stub = (ULONG*)AllocVec32(64, MEMF_PUBLIC | MEMF_CLEAR))) {
            Printf("no PPC memory\n");
            CloseLibrary(PowerPCBase);
            return 10;
        }
        stub[0] = 0x38600000;   /* li r3,0 */
        stub[1] = 0x4E800020;   /* blr */
        memset(&args, 0, sizeof(args));
        args.PP_Code = stub;
        Printf("  RunPPC: %ld\n", (LONG)RunPPC(&args));
        Flush(Output());
        state("after RunPPC");
        FreeVec32(stub);
    }
    start(async);
    state("after second start");
    start(async);
    state("after third start");
    CloseLibrary(PowerPCBase);
    Printf("done\n");
    return 0;
}
