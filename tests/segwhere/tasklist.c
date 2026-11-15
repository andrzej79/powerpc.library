/* tasklist - every task with its TF_PPC bit and where its structure is (17.17 test)
 * Build: vc +aos68k -c99 -O1 tasklist.c -o tasklist -lamiga */
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
    for (mh = (struct MemHeader*)SysBase->MemList.lh_Head; mh->mh_Node.ln_Succ; mh = (struct MemHeader*)mh->mh_Node.ln_Succ)
        if ((p >= mh->mh_Lower) && (p < mh->mh_Upper))
            return (mh->mh_Attributes & MEMF_PPC) ? "CARD" : ((mh->mh_Attributes & MEMF_CHIP) ? "chip" : "fast");
    return "?";
}

static struct { struct Task* t; char name[40]; UBYTE flags; const char* w; APTR sp; } tab[96];
static int n;

static void grab(struct List* l)
{
    struct Node* nd;
    for (nd = l->lh_Head; nd->ln_Succ && n < 96; nd = nd->ln_Succ) {
        struct Task* t = (struct Task*)nd;
        const char* s = nd->ln_Name ? nd->ln_Name : "-";
        int i;
        if (t->tc_Node.ln_Type == NT_PROCESS) {
            struct CommandLineInterface* cli = BADDR(((struct Process*)t)->pr_CLI);
            if (cli && cli->cli_Module && cli->cli_CommandName) {
                UBYTE* b = BADDR(cli->cli_CommandName);
                if (b[0]) { static char cn[96][40]; int k; for (k = 0; k < b[0] && k < 39; k++) cn[n][k] = b[k + 1]; cn[n][k] = 0; s = cn[n]; }
            }
        }
        for (i = 0; s[i] && i < 39; i++) tab[n].name[i] = s[i];
        tab[n].name[i] = 0;
        tab[n].t = t; tab[n].flags = t->tc_Flags; tab[n].w = where(t); tab[n].sp = t->tc_SPReg;
        n++;
    }
}

int main(void)
{
    int i;
    Disable();
    grab(&SysBase->TaskReady);
    grab(&SysBase->TaskWait);
    Enable();
    for (i = 0; i < n; i++)
        Printf("%-32s TF_PPC=%ld task %s stack %s\n", (LONG)tab[i].name, (LONG)((tab[i].flags & TF_PPC) ? 1 : 0), (LONG)tab[i].w, (LONG)where(tab[i].sp));
    return 0;
}
