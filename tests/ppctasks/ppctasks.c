/* ppctasks: every PPC task the library knows (pp_AllTasks), read from the 68k side - state, signals,
 * scheduler fields and CPU time, and which task is running. For a hang: which PPC task waits for what,
 * and which one is eating the CPU (run it twice and compare "total"). Built against the library's own
 * headers, so the PrivatePPCBase layout is the one the library was built with. */
#include <exec/exec.h>
#include <dos/dos.h>
#include <powerpc/powerpc.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include "constants.h"
#include "libstructs.h"

#define MAXT 48
static struct {
    struct TaskPPC *t;
    char name[40];
    UBYTE state; BYTE pri;
    ULONG wait, recvd, alloc, flags, elapsed, total, quantum, prio, prioff, desired, nice, activity, busy;
    char where;
} t[MAXT];

static struct { struct TaskPPC *task; ULONG hi, lo; } wt[16];
static int nw;
static ULONG flagWait, curTBL;

static char inList(struct MinList *l, struct TaskPPC *task)
{
    struct MinNode *n;
    for (n = l->mlh_Head; n->mln_Succ; n = n->mln_Succ) {
        if ((struct TaskPPC *)n == task) return 1;
    }
    return 0;
}

int main(void)
{
    struct PrivatePPCBase *pb = (struct PrivatePPCBase *)OpenLibrary("powerpc.library", 15);
    struct MinNode *n;
    struct TaskPPC *run;
    int cnt = 0, i;

    if (!pb) { printf("no powerpc.library\n"); return 20; }
    Disable();
    run = pb->pp_ThisPPCProc;
    for (n = pb->pp_AllTasks.mlh_Head; n->mln_Succ && cnt < MAXT; n = n->mln_Succ) {
        struct TaskPPC *p = ((struct TaskPtr *)n)->tptr_Task;
        char *nm = p->tp_Task.tc_Node.ln_Name;
        t[cnt].t = p;
        for (i = 0; i < 39 && nm && nm[i]; i++) t[cnt].name[i] = nm[i];
        t[cnt].name[i] = 0;
        t[cnt].state = p->tp_Task.tc_State; t[cnt].pri = p->tp_Task.tc_Node.ln_Pri;
        t[cnt].wait = p->tp_Task.tc_SigWait; t[cnt].recvd = p->tp_Task.tc_SigRecvd; t[cnt].alloc = p->tp_Task.tc_SigAlloc;
        t[cnt].flags = p->tp_Flags; t[cnt].elapsed = p->tp_Elapsed; t[cnt].total = p->tp_Totalelapsed;
        t[cnt].quantum = p->tp_Quantum; t[cnt].prio = p->tp_Priority; t[cnt].prioff = p->tp_Prioffset;
        t[cnt].desired = p->tp_Desired; t[cnt].nice = p->tp_Nice; t[cnt].activity = p->tp_Activity; t[cnt].busy = p->tp_Busy;
        t[cnt].where = (p == run) ? 'R' : inList(&pb->pp_ReadyTasks, p) ? 'r' : inList(&pb->pp_WaitingTasks, p) ? 'w'
                     : inList(&pb->pp_NewTasks, p) ? 'n' : '?';
        cnt++;
    }
    {
        struct WaitTime *w;
        nw = 0;
        flagWait = pb->pp_FlagWait;
        curTBL = pb->pp_CurrentTBL;
        for (w = (struct WaitTime *)pb->pp_WaitTime.mlh_Head; w->wt_Node.ln_Succ && nw < 16; w = (struct WaitTime *)w->wt_Node.ln_Succ) {
            wt[nw].task = w->wt_Task; wt[nw].hi = w->wt_TimeUpper; wt[nw].lo = w->wt_TimeLower; nw++;
        }
    }
    Enable();
    printf("%-28s L st pri  sigwait  recvd    alloc    tpflags  quantum  elapsed  total    prio     prioff   desired  nice act  busy\n", "PPC task");
    for (i = 0; i < cnt; i++) {
        printf("%-28s %c %2d %3d %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx %4ld %4lu %4lu\n",
               t[i].name, t[i].where, t[i].state, t[i].pri, t[i].wait, t[i].recvd, t[i].alloc, t[i].flags, t[i].quantum,
               t[i].elapsed, t[i].total, t[i].prio, t[i].prioff, t[i].desired, (LONG)t[i].nice, t[i].activity, t[i].busy);
    }
    printf("StdQuantum %lu Quantum %lu LowActivityPri %ld Offset %ld\n", pb->pp_StdQuantum, pb->pp_Quantum,
           pb->pp_LowActivityPri, pb->pp_LowActivityPriOffset);
    printf("FlagWait %lu CurrentTBL %08lx, WaitTime list %d:\n", flagWait, curTBL, nw);
    for (i = 0; i < nw; i++) {
        int j;
        char *nm = "?";
        for (j = 0; j < cnt; j++) if (t[j].t == wt[i].task) nm = t[j].name;
        printf("  %-28s deadline %08lx:%08lx (lo - CurrentTBL %ld)\n", nm, wt[i].hi, wt[i].lo, (LONG)(wt[i].lo - curTBL));
    }
    CloseLibrary((struct Library *)pb);
    return 0;
}
