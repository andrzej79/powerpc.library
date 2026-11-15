// Copyright (c) 2019-2020, 2024 Dennis van der Boon
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <exec/exec.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <utility/tagitem.h>
#include <dos/dostags.h>
#include <dos/var.h>
#include <powerpc/powerpc.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/prometheus.h>
#include <dos/doshunks.h>
#include <hardware/custom.h>
#include <hardware/intbits.h>

#include "constants.h"
#include "libstructs.h"
#include "Internals68k.h"

extern APTR OldLoadSeg, OldNewLoadSeg, OldAllocMem, OldAddTask, OldRemTask, OldUnLoadSeg;
extern struct ExecBase* mySysBase;
extern struct PPCBase* myPPCBase;

APTR  RemSysTask;

/*
 * Q10: SET WHILE GortInt SERVES THE MESSAGE QUEUE. ZenInt, the vertical-blank
 * server of the K1 and the M1s, runs at level 3 and can interrupt GortInt, an
 * INT2 server at level 2; a second GortInt inside the first would serve the
 * frames behind the one in hand first. ZenInt leaves the queue alone while
 * this is set, and a GortInt entered while it is set returns at once. A 68k
 * variable, so that testing it costs GortInt no read across the bus: the
 * library's base is in the card's memory. Zero at load (BSS).
 */
static volatile ULONG gortBusy;

UBYTE powerlib[] = "powerpc.library\0";
UBYTE ppclib[] = "ppc.library\0";
UBYTE ampname[]  = "AmigaAMP\0";
UBYTE testlongs[] = "2005_68K_PPCsk_0sk_1\0\0\0\0";
UBYTE CrashMessage[] =	 
            "Task name: '%s'  Task address: %08lx\n"
			"Exception: %s\n\n"
			"SRR0: %08lx    SRR1:  %08lx     MSR:   %08lx    HID0: %08lx\n"
			"PVR:  %08lx    DAR:   %08lx     DSISR: %08lx    SDR1: %08lx\n"
			"DEC:  %08lx    TBU:   %08lx     TBL:   %08lx    XER:  %08lx\n"
			"CR:   %08lx    FPSCR: %08lx     LR:    %08lx    CTR:  %08lx\n\n"
			"R0-R3:   %08lx %08lx %08lx %08lx   IBAT0: %08lx %08lx\n"
			"R4-R7:   %08lx %08lx %08lx %08lx   IBAT1: %08lx %08lx\n"
			"R8-R11:  %08lx %08lx %08lx %08lx   IBAT2: %08lx %08lx\n"
			"R12-R15: %08lx %08lx %08lx %08lx   IBAT3: %08lx %08lx\n"
			"R16-R19: %08lx %08lx %08lx %08lx   DBAT0: %08lx %08lx\n"
			"R20-R23: %08lx %08lx %08lx %08lx   DBAT1: %08lx %08lx\n"
			"R24-R27: %08lx %08lx %08lx %08lx   DBAT2: %08lx %08lx\n"
			"R28-R31: %08lx %08lx %08lx %08lx   DBAT3: %08lx %08lx\n\n\0";

UBYTE msgPanic[] = "Kernel Panic!\0";

UBYTE excSysReset[]    = "System Reset\0";
UBYTE excMCheck[]      = "Machine Check\0";
UBYTE excDAccess[]     = "Data Storage\0";
UBYTE excIAccess[]     = "Instruction Storage\0";
UBYTE excInterrupt[]   = "External\0";
UBYTE excAlign[]       = "Alignment\0";
UBYTE excProgram[]     = "Program\0";
UBYTE excFPUn[]        = "FPU Unavailable\0";
UBYTE excDecrementer[] = "Decrementer\0";
UBYTE excAltivecUnav[] = "AltiVec Unavailable\0";
UBYTE excSC[]          = "System Call\0";
UBYTE excTrace[]       = "Trace\0";
UBYTE excFPAssist[]    = "Floating Point Assist\0";
UBYTE excPerfMon[]     = "Performance Monitor\0";
UBYTE excIABR[]        = "Instruction Breakpoint\0";
UBYTE excSysMan[]      = "System Management\0";
UBYTE excAVAssist[]    = "AltiVec Assist\0";
UBYTE excTherMan[]     = "Thermal Management\0";
UBYTE excUnsupported[] = "Unsupported\0";

ULONG ExcStrings[24]   = {(ULONG)&excUnsupported, (ULONG)&excUnsupported, (ULONG)&excMCheck,
                          (ULONG)&excDAccess, (ULONG)&excIAccess, (ULONG)&excInterrupt,
                          (ULONG)&excAlign, (ULONG)&excProgram, (ULONG)&excFPUn, (ULONG)&excDecrementer,
                          (ULONG)&excAltivecUnav, (ULONG)&excUnsupported, (ULONG)&excSC,
                          (ULONG)&excTrace, (ULONG)&excFPAssist, (ULONG)&excPerfMon,
                          (ULONG)&excUnsupported, (ULONG)&excUnsupported, (ULONG)&excUnsupported,
                          (ULONG)&excIABR, (ULONG)&excSysMan, (ULONG)&excUnsupported,
                          (ULONG)&excAVAssist, (ULONG)&excTherMan};


UBYTE FRun68K[]                      = "Run68K\0";
UBYTE FWaitFor68K[]                  = "WaitFor68K\0";
UBYTE FAllocVecPPC[]                 = "AllocVecPPC\0";
UBYTE FFreeVecPPC[]                  = "FreeVecPPC\0";
UBYTE FCreateTaskPPC[]               = "CreateTaskPPC\0";
UBYTE FDeleteTaskPPC[]               = "DeleteTaskPPC\0";
UBYTE FFindTaskPPC[]                 = "FindTaskPPC\0";
UBYTE FInitSemaphorePPC[]            = "InitSemaphorePPC\0";
UBYTE FFreeSemaphorePPC[]            = "FreeSemaphorePPC\0";
UBYTE FAddSemaphorePPC[]             = "AddSemaphorePPC\0";
UBYTE FRemSemaphorePPC[]             = "RemSemaphorePPC\0";
UBYTE FObtainSemaphorePPC[]          = "ObtainSemaphorePPC\0";
UBYTE FAttemptSemaphorePPC[]         = "AttemptSemaphorePPC\0";
UBYTE FReleaseSemaphorePPC[]         = "ReleaseSemaphorePPC\0";
UBYTE FFindSemaphorePPC[]            = "FindSemaphorePPC\0";
UBYTE FAllocSignalPPC[]              = "AllocSignalPPC\0";
UBYTE FFreeSignalPPC[]               = "FreeSignalPPC\0";
UBYTE FSetSignalPPC[]                = "SetSignalPPC\0";
UBYTE FSignalPPC[]                   = "SignalPPC\0";
UBYTE FWaitPPC[]                     = "WaitPPC\0";
UBYTE FSetTaskPriPPC[]               = "SetTaskPriPPC\0";
UBYTE FSignal68K[]                   = "Signal68K\0";
UBYTE FSetCache[]                    = "SetCache\0";
UBYTE FSetExcHandler[]               = "SetExcHandler\0";
UBYTE FRemExcHandler[]               = "RemExcHandler\0";
UBYTE FSetHardware[]                 = "SetHardware\0";
UBYTE FModifyFPExc[]                 = "ModifyFPExc\0";
UBYTE FWaitTime[]                    = "WaitTime\0";
UBYTE FChangeStack[]                 = "ChangeStack\0";
UBYTE FChangeMMU[]                   = "ChangeMMU\0";
UBYTE FGetInfo[]                     = "GetInfo\0";
UBYTE FCreateMsgPortPPC[]            = "CreateMsgPortPPC\0";
UBYTE FDeleteMsgPortPPC[]            = "DeleteMsgPortPPC\0";
UBYTE FAddPortPPC[]                  = "AddPortPPC\0";
UBYTE FRemPortPPC[]                  = "RemPortPPC\0";
UBYTE FFindPortPPC[]                 = "FindPortPPC\0";
UBYTE FWaitPortPPC[]                 = "WaitPortPPC\0";
UBYTE FPutMsgPPC[]                   = "PutMsgPPC\0";
UBYTE FGetMsgPPC[]                   = "GetMsgPPC\0";
UBYTE FReplyMsgPPC[]                 = "ReplyMsgPPC\0";
UBYTE FCopyMemPPC[]                  = "CopyMemPPC\0";
UBYTE FAllocXMsgPPC[]                = "AllocXMsgPPC\0";
UBYTE FFreeXMsgPPC[]                 = "FreeXMsgPPC\0";
UBYTE FPutXMsgPPC[]                  = "PutXMsgPPC\0";
UBYTE FGetSysTimePPC[]               = "GetSysTimePPC\0";
UBYTE FSetReplyPortPPC[]             = "SetReplyPortPPC\0";
UBYTE FSnoopTask[]                   = "SnoopTask\0";
UBYTE FEndSnoopTask[]                = "EndSnoopTask\0";
UBYTE FGetHALInfo[]                  = "GetHALInfo\0";
UBYTE FSetScheduling[]               = "SetScheduling\0";
UBYTE FFindTaskByID[]                = "FindTaskByID\0";
UBYTE FSetNiceValue[]                = "SetNiceValue\0";
UBYTE FTrySemaphorePPC[]             = "TrySemaphorePPC\0";
UBYTE FSetExceptPPC[]                = "SetExceptPPC\0";
UBYTE FObtainSemaphoreSharedPPC[]    = "ObtainSemaphoreSharedPPC\0";
UBYTE FAttemptSemaphoreSharedPPC[]   = "AttemptSempahoreSharedPPC\0";
UBYTE FProcurePPC[]                  = "ProcurePPC\0";
UBYTE FVacatePPC[]                   = "VacatePPC\0";
UBYTE FCreatePoolPPC[]               = "CreatePoolPPC\0";
UBYTE FDeletePoolPPC[]               = "DeletePoolPPC\0";
UBYTE FAllocPooledPPC[]              = "AllocPooledPPC\0";
UBYTE FFreePooledPPC[]               = "FreePooledPPC\0";
UBYTE FRawDoFmtPPC[]                 = "RawDoFmtPPC\0";
UBYTE FPutPublicMsgPPC[]             = "PutPublicMsgPPC\0";
UBYTE FAddUniquePortPPC[]            = "AddUniquePortPPC\0";
UBYTE FAddUniqueSemaphorePPC[]       = "AddUniqueSemaphorePPC\0";

UBYTE FAllocatePPC[]                 = "AllocatePPC\0";
UBYTE FDeallocatePPC[]               = "DeallocatePPC\0";

#if 0
UBYTE FSPrintF[]                     = "SPrintF\0";
UBYTE FInsertPPC[]                   = "InsertPPC\0";
UBYTE FAddHeadPPC[]                  = "AddHeadPPC\0";
UBYTE FAddTailPPC[]                  = "AddtailPPC\0";
UBYTE FRemovePPC[]                   = "RemovePPC\0";
UBYTE FRemHeadPPC[]                  = "RemHeadPPC\0";
UBYTE FRemTailPPC[]                  = "RemTailPPC\0";
UBYTE FEnqueuePPC[]                  = "EnqueuePPC\0";
UBYTE FFindNamePPC[]                 = "FindNamePPC\0";
UBYTE FFindTagItemPPC[]              = "FindTagItemPPC\0";
UBYTE FGetTagDataPPC[]               = "GetTagItemPPC\0";
UBYTE FNextTagItemPPC[]              = "NextTagItemPPC\0";
UBYTE FSuper[]                       = "Super\0";
UBYTE FUser[]                        = "User\0";
UBYTE FLockTaskList[]                = "LockTaskList\0";
UBYTE FUnLockTaskList[]              = "UnlockTaskList\0";
UBYTE FSetExcMMU[]                   = "SetExcMMU\0";
UBYTE FClearExcMMU[]                 = "ClearExcMMU\0";
UBYTE FFreeAllMem[]                  = "FreeAllMem\0";
UBYTE FAddTimePPC[]                  = "AddTimePPC\0";
UBYTE FSubTimePPC[]                  = "SubTimePPC\0";
UBYTE FCmpTimePPC[]                  = "CmpTimePPC\0";
UBYTE FNewListPPC[]                  = "NewListPPC\0";
UBYTE FCauseInterrupt[]              = "CauseInterrupt\0";
UBYTE FAllocPrivateMem[]             = "AllocPrivateMem\0";
UBYTE FFreePrivateMem[]              = "FreePrivateMem\0";
UBYTE FResetPPC[]                    = "ResetPPC\0";
UBYTE FIsExceptionMode[]             = "IsExceptionMode\0";
UBYTE FRun68KLowLevel[]              = "Run68KLowLevel\0";
#endif

ULONG FuncStrings[68]   = {(ULONG)&FRun68K, (ULONG)&FWaitFor68K, (ULONG)&FAllocVecPPC,
                          (ULONG)&FFreeVecPPC, (ULONG)&FCreateTaskPPC, (ULONG)&FDeleteTaskPPC,
                          (ULONG)&FFindTaskPPC, (ULONG)&FInitSemaphorePPC, (ULONG)&FFreeSemaphorePPC,
                          (ULONG)&FAddSemaphorePPC, (ULONG)&FRemSemaphorePPC, (ULONG)&FObtainSemaphorePPC,
                          (ULONG)&FAttemptSemaphorePPC, (ULONG)&FReleaseSemaphorePPC, (ULONG)&FFindSemaphorePPC,
                          (ULONG)&FAllocSignalPPC, (ULONG)&FFreeSignalPPC, (ULONG)&FSetSignalPPC,
                          (ULONG)&FSignalPPC, (ULONG)&FWaitPPC, (ULONG)&FSetTaskPriPPC,
                          (ULONG)&FSignal68K, (ULONG)&FSetCache, (ULONG)&FSetExcHandler,
                          (ULONG)&FRemExcHandler, (ULONG)&FSetHardware, (ULONG)&FModifyFPExc,
                          (ULONG)&FWaitTime, (ULONG)&FChangeStack, (ULONG)&FChangeMMU,
                          (ULONG)&FGetInfo, (ULONG)&FCreateMsgPortPPC, (ULONG)&FDeleteMsgPortPPC,
                          (ULONG)&FAddPortPPC, (ULONG)&FRemPortPPC, (ULONG)&FFindPortPPC,
                          (ULONG)&FWaitPortPPC, (ULONG)&FPutMsgPPC, (ULONG)&FGetMsgPPC,
                          (ULONG)&FReplyMsgPPC, (ULONG)&FCopyMemPPC, (ULONG)&FAllocXMsgPPC,
                          (ULONG)&FFreeXMsgPPC, (ULONG)&FPutXMsgPPC, (ULONG)&FGetSysTimePPC,
                          (ULONG)&FSetReplyPortPPC, (ULONG)&FSnoopTask, (ULONG)&FEndSnoopTask,
                          (ULONG)&FGetHALInfo, (ULONG)&FSetScheduling, (ULONG)&FFindTaskByID,
                          (ULONG)&FSetNiceValue, (ULONG)&FTrySemaphorePPC, (ULONG)&FSetExceptPPC,
                          (ULONG)&FObtainSemaphoreSharedPPC, (ULONG)&FAttemptSemaphoreSharedPPC, (ULONG)&FProcurePPC,
                          (ULONG)&FVacatePPC, (ULONG)&FCreatePoolPPC, (ULONG)&FDeletePoolPPC,
                          (ULONG)&FAllocPooledPPC, (ULONG)&FFreePooledPPC, (ULONG)&FRawDoFmtPPC,
                          (ULONG)&FPutPublicMsgPPC, (ULONG)&FAddUniquePortPPC, (ULONG)&FAddUniqueSemaphorePPC,
                          (ULONG)&FAllocatePPC,(ULONG)FDeallocatePPC};

#ifdef DEBUG

ULONG dsi_Count = 0;

struct dsiInfo
{
    ULONG dsi_Lower;
    ULONG dsi_Upper;
    ULONG dsi_Total;
};

struct dsiInfo di[10];


ULONG countDSI(__reg("d0") ULONG dsi_Address)
{
    ULONG init = 0;
    ULONG result = 1;
    struct PPCBase* PowerPCBase = myPPCBase;

    if (!(dsi_Count))
    {
        struct ExecBase* SysBase = mySysBase;
        struct MemHeader* currMem = (struct MemHeader*)SysBase->MemList.lh_Head;
        struct MemHeader* nextMem;

        while (nextMem = (struct MemHeader*)currMem->mh_Node.ln_Succ)
        {
            di[init].dsi_Lower = (ULONG)(currMem->mh_Lower);
            di[init].dsi_Upper = (ULONG)(currMem->mh_Upper);
            di[init].dsi_Total = 0;

            if (init++ == 9)
            {
                break;
            }
            currMem = nextMem;
        }

        while (init < 10)
        {
            di[init].dsi_Lower = 0;
            di[init].dsi_Upper = 0;
            di[init].dsi_Total = 0;
            init++;
        }
    }

    init = 0;

    while (di[init].dsi_Lower)
    {
        if ((di[init].dsi_Lower <= dsi_Address) && (di[init].dsi_Upper >= dsi_Address))
        {
            di[init].dsi_Total += 1;
            break;
        }
        init++;
    }

    if (!(di[init].dsi_Lower))
    {
        if (dsi_Address != 4)
        {
            di[9].dsi_Total += 1;
            result = 0;
            mySPrintF68K((struct PPCBase*)PowerPCBase, "Detected DSI hit outside of memory bounds at %08lx\n\0", (APTR)&dsi_Address);
        }
    }

    dsi_Count += 1;

    if (!(dsi_Count & 0x3ff))
    {
        mySPrintF68K((struct PPCBase*)PowerPCBase, "Overview of previous 1024 DSI hits\n\0",0);

        init = 0;
        while (di[init].dsi_Lower)
        {
             mySPrintF68K((struct PPCBase*)PowerPCBase, "DSI Count %08lx - %08lx: %ld\n\0", (APTR)&di[init]);
             init += 1;
        }

        if (di[9].dsi_Total)
        {
             mySPrintF68K((struct PPCBase*)PowerPCBase, "DSI Count unknown: %ld\n\0", (APTR)&di[9].dsi_Total);
        }

        mySPrintF68K((struct PPCBase*)PowerPCBase, "End of report\n\0",0);

        dsi_Count = 0;
    }
    return result;
}

#endif


/********************************************************************************************
*
*	Task patch functions. To remove PPC tasks and free memory when the 68K part is ended
*
*********************************************************************************************/

/*
 * M7 (REVIEW-LIB.md): THE MIRROR LIST IS WALKED UNDER FORBID. RunPPC, WaitForPPC
 * and commonRemTask followed pp_MirrorList from its head without a lock, while an
 * exiting process took its own entry off under Disable and freed it after the
 * Enable (DeleteMsgPort, FreeVec). A walker that exec had switched out standing
 * on that entry resumed on freed memory. The entry is an AllocVec block, which
 * keeps a size longword in front of it, so FreeMem wrote the chunk size, 32, over
 * mln_Succ: the walker took the low-memory vector area for the next node and
 * followed code words as pointers (a bus error or a Guru on most machines); with
 * the chunk reused it read 0 and the walk ended early, and RunPPC then made a
 * second entry and a second PPC task for its caller, and WaitForPPC answered
 * PPERR_WAITERR with a call still out. A reader under Forbid cannot be switched
 * out while it holds a pointer into the list, and a remover's Forbid section
 * cannot overlap a reader's, so the free after the Permit meets no reader.
 *
 * 17.13b walks without a lock in three places (powerpc.s:3592-3603 RunPPC,
 * :3822-3832 WaitForPPC, :3121-3128 the RemTask exit code), removes under
 * Disable (:3135-3140) and frees after the Enable (:3142-3146); WarpOS walks its
 * Core list between Forbid and Permit (FindCoreEntry, powerpc.s:3673-3695) and
 * removes under Forbid (:5046-5049): THIS DIVERGES from 17.13b and MATCHES
 * WarpOS. The entry returned is the caller's own, which only the caller's exit
 * frees, so the caller uses it outside the Forbid. Test: tests/mirrorrace.
 */
struct MirrorTask* FindMirror(struct PrivatePPCBase* PowerPCBase, struct Task* task)
{
    struct ExecBase* SysBase = PowerPCBase->pp_PowerPCBase.PPC_SysLib;
    struct MirrorTask* found = NULL;
    struct MirrorTask* currMir;
    struct MirrorTask* nxtMir;

    Forbid();

    currMir = (struct MirrorTask*)PowerPCBase->pp_MirrorList.mlh_Head;

    while (nxtMir = (struct MirrorTask*)currMir->mt_Node.mln_Succ)
    {
        if (currMir->mt_Task == task)
        {
            found = currMir;
            break;
        }
        currMir = nxtMir;
    }

    Permit();

    return found;
}

void commonRemTask(__reg("a1") struct Task* myTask, __reg("d0") ULONG flag, __reg("a6") struct ExecBase* SysBase)
{
    if (!(myTask))
    {
        myTask = SysBase->ThisTask;
    }

    if (myTask->tc_Node.ln_Type == NT_PROCESS)
    {
        struct PrivatePPCBase* PowerPCBase = (struct PrivatePPCBase*)myPPCBase;
        struct MirrorTask* currMir;
        struct MirrorTask* nxtMir;
        struct MirrorTask* found = NULL;

        /*
         * M7: THE ENTRY IS FOUND AND TAKEN OFF IN ONE FORBID SECTION, and what
         * needs the 68k's time comes after it -- the END frame (CreateMsgFrame
         * can wait for the PPC to free a frame, Q4), DeleteMsgPort and FreeVec. A
         * second remover of the same task (the owner exiting while another
         * process kills it) finds nothing, where it used to find the entry still
         * listed while this one was waiting for a frame, and freed it twice.
         * 17.13b sends the END frame first and removes after it (powerpc.s:3130-3140);
         * the entry is off the list before the PPC task is told now.
         */
        Forbid();

        currMir = (struct MirrorTask*)PowerPCBase->pp_MirrorList.mlh_Head;

        while (nxtMir = (struct MirrorTask*)currMir->mt_Node.mln_Succ)
        {
            if (currMir->mt_Task == myTask)
            {
                Remove((struct Node*)currMir);
                found = currMir;
                break;
            }
            currMir = nxtMir;
        }

        Permit();

        if (found)
        {
            /*
             * ONLY A PPC TASK THAT EXISTS IS ENDED -- M3 (REVIEW-LIB.md). An
             * entry whose first call never came back has no PPC task, and
             * HandleMsgs (kernel.c) routes ID_END through mf_PPCTask: NULL
             * was dereferenced there. NO COUNTERPART in WarpOS, whose entry
             * always holds its PPC process (CreatePPCProc runs before
             * CreateCoreEntry, powerpc.s:3754-3755, :3816) and whose RemTask
             * patch signals it (:5039-5041).
             */
            if ((!(flag)) && (found->mt_PPCTask))
            {
                struct MsgFrame* myFrame = CreateMsgFrame(PowerPCBase);
                myFrame->mf_Identifier = ID_END;
                myFrame->mf_PPCTask = found->mt_PPCTask;
                SendMsgFrame(PowerPCBase, myFrame);
            }
            DeleteMsgPort(found->mt_Port);
            FreeVec(found);
        }
    }
    return;
}

/********************************************************************************************/

/*
 * P2 (REVIEW-LIB.md, 10 Nov 2026): THE CLEANUP OF SysExitCode.
 *
 * patchAddTask gives every process whose own final PC lies in ROM (a zero high
 * byte) the routine SysExitCode, in functions68k.s, so that the process's PPC
 * mirror is removed before the original runs. dos's exit routine takes the
 * process's return code in d0 and expects the stack as the process's last rts
 * left it. This used to be one C function: it set d0 to 0 for commonRemTask,
 * which left d0 as it liked (still 0, unless a mirror was freed), and entered
 * the original with jsr, which put a return address and three saved registers
 * (16 bytes) on the stack. Now SysExitCode saves d0-a6, calls this routine,
 * restores d0-a6 and enters the original by push and rts, as 17.13b's
 * ExitCode2 does (powerpc.s:3106-3111). WarpOS has no counterpart: its AddTask
 * patch only supplies a termination routine for a NULL final PC
 * (powerpc.s:4996-5012). tests/patches68k.c.
 */
PATCH68K void SysExitHelper(void)
{
    struct ExecBase* SysBase = myPPCBase->PPC_SysLib;

    commonRemTask(SysBase->ThisTask, 0, SysBase);

    return;
}

/********************************************************************************************/

PATCH68K void patchRemTask(__reg("a1") struct Task* myTask, __reg("a6") struct ExecBase* SysBase)
{
    void (*RemTask_ptr)(__reg("a1") struct Task*, __reg("a6") struct ExecBase*) = OldRemTask;

    commonRemTask (myTask, 0, SysBase);

    RemTask_ptr(myTask, SysBase);

    return;
}

/********************************************************************************************/

PATCH68K APTR patchAddTask(__reg("a1") struct Task* myTask, __reg("a2") APTR initialPC,
                  __reg("a3") APTR finalPC, __reg("a6") struct ExecBase* SysBase)
{
    APTR (*AddTask_ptr)(__reg("a1") struct Task*, __reg("a2") APTR,
    __reg("a3") APTR, __reg("a6") struct ExecBase*) = OldAddTask;

    if (myTask->tc_Node.ln_Type == NT_PROCESS)
    {
        if (!(finalPC))
        {
            finalPC = (APTR)&patchRemTask;
        }
        else
        {
            if (!((ULONG)finalPC & 0xff000000))
            {
                RemSysTask = finalPC;
                finalPC = (APTR)&SysExitCode;
            }
        }
    }
    return ((*AddTask_ptr)(myTask, initialPC, finalPC, SysBase));
}

/********************************************************************************************
*
*	Allocate memory patch to load PPC (related) code to PPC memory
*
*********************************************************************************************/

/*
 * P1 (REVIEW-LIB.md, 10 Nov 2026): this is 17.13b's NewAlloc again
 * (powerpc.s:3251-3340), less its A1200 patches (TB_WARN), which the library
 * does not install. The port to C had lost four things:
 *
 *  - the filter let every request through that had FAST in it, whatever else it
 *    asked for (powerpc.s:3252-3260, "If FAST requested, redirect"); here a
 *    request without PUBLIC returned at once, so a task that asked for
 *    MEMF_FAST alone never got PPC memory;
 *  - a task with no name made the walk read at 0xFFFFFFFB (a NULL pointer, then
 *    -5); 17.13b exits there (powerpc.s:3288-3289). Here a task with no name
 *    skips the name tests and keeps the tagging test at the end, as a CLI
 *    process with no command name always did;
 *  - the walk stopped ON the NUL, so the four bytes compared were the ones
 *    before the last character, for every task that is not a CLI process;
 *    17.13b steps past the NUL and reads at -5, the last four
 *    (powerpc.s:3292-3294). A name under four characters is not compared
 *    (17.13b would read before the string);
 *  - the AmigaAMP routine, for a name ending in "aAMP" (powerpc.s:3232-3249 and
 *    :3297-3298).
 *
 * WarpOS has no AllocMem patch. tests/patches68k.c.
 */
PATCH68K APTR patchAllocMem(__reg("d0") ULONG byteSize, __reg("d1") ULONG attributes,
                   __reg("a6") struct ExecBase* SysBase)
{
    APTR (*AllocMem_ptr)(__reg("d0") ULONG, __reg("d1") ULONG, __reg("a6") struct ExecBase*) = OldAllocMem;

    USHORT testAttr = (USHORT)attributes;

    if (testAttr && !(testAttr & MEMF_FAST))
    {
        if ( (testAttr & MEMF_CHIP) || !(testAttr & MEMF_PUBLIC))
        {
            return ((*AllocMem_ptr)(byteSize, attributes, SysBase));
        }
    }               //TB_WARN (A1200) not yet implemented

    struct Task* myTask = SysBase->ThisTask;
    char * myName = NULL;
    ULONG nameLen = 0;

    ULONG highTaskByte = (ULONG)(myTask) & 0xf0000000;

    if (highTaskByte)
    {
        highTaskByte &= 0xe0000000;
        struct PrivatePPCBase *privBase = (struct PrivatePPCBase*) myPPCBase;
        ULONG highBaseByte = (ULONG)(privBase->pp_PPCMemBase) & 0xe0000000;
        if (!(highTaskByte ^= highBaseByte))
        {
            myTask->tc_Flags |= TF_PPC;
        }
    }

    struct Process* myProcess = (struct Process*)myTask;

    if ((myTask->tc_Node.ln_Type != NT_PROCESS) || ((myTask->tc_Node.ln_Type == NT_PROCESS) && ((myProcess->pr_CLI == NULL))))
    {
        char* nameStart = myTask->tc_Node.ln_Name;

        if (nameStart)
        {
            char* nameEnd = nameStart;

            while (nameEnd[0] != 0)
            {
                nameEnd ++;
            }
            nameLen = nameEnd - nameStart;

            if (nameLen >= 4)
            {
                myName = nameEnd - 4;
            }
        }
    }
    else
    {
        struct CommandLineInterface* myCLI;

        if (myCLI = (struct CommandLineInterface *)((ULONG)(myProcess->pr_CLI << 2)))
        {
            if (myName = (char *)(ULONG)(myCLI->cli_CommandName << 2))
            {
                UBYTE offset = myName[0] - 3;
                nameLen = (UBYTE)myName[0];
                if (!(offset & 0x80))
                {
                    myName += offset;
                }
            }
        }
    }

    ULONG testValue;
    ULONG longnum = 0;

    if (myName)
    {
        ULONG lastFour = *((ULONG*)myName);

        while (testValue = *((ULONG*)testlongs + longnum))
        {
            if (testValue == lastFour)
            {
                attributes |= MEMF_PPC;
                return ((*AllocMem_ptr)(byteSize, attributes, SysBase));
            }
        longnum += 1;
        }

        if (lastFour == 0x61414D50)                 //"aAMP", AmigaAMP (powerpc.s:3232-3249, :3297-3298)
        {
            if (!(myTask->tc_Flags & TF_PPC))       //a task tagged by this library goes to PPC memory
            {
                if ((byteSize != 0xC094) && (byteSize != 0x904) && (byteSize != 0x4D4))     //the sizes of AmigaAMP v3's buffers
                {
                    if (((LONG)byteSize < 0x4000) || ((byteSize & 0xFF) != 0x54))           //AmigaAMP v2's lazy patch: 0x4000 or more, ending in 0x54
                    {
                        return ((*AllocMem_ptr)(byteSize, attributes, SysBase));
                    }
                }
            }
            attributes |= MEMF_PPC;
            return ((*AllocMem_ptr)(byteSize, attributes, SysBase));
        }

        if ((lastFour == 0x70656564) || ((lastFour == 0x2e657865) && (nameLen >= 8) && (*((ULONG*)myName - 1) == 0x70616365)))
        {
            return ((*AllocMem_ptr)(byteSize, attributes, SysBase));
        }

    }

    if (myTask->tc_Flags & TF_PPC)
    {
        attributes |= MEMF_PPC;
    }
    return ((*AllocMem_ptr)(byteSize, attributes, SysBase));
}

/********************************************************************************************
*
*	Executable loading patches: where a program's hunks go (17.17)
*   (AUTODOCS ARE WRONG, the InternalLoadSeg functions are called with d1, d2, d3 and not
*   d1, a0, d0)
*
*   The 68k reads the card's memory uncached (libinit.c refuses to start otherwise, it is
*   how the two CPUs stay coherent), so 68k code, a 68k stack or 68k buffers there run at
*   bus speed: on an A3000 with a Firebird and a Harrier, about 5 MB/s. Up to 17.16 every
*   program that held the string "powerpc.library", "ppc.library", "AmigaAMP" or "RACE"
*   was loaded with every hunk but the first in PPC memory, and the process that loaded it
*   was left tagged (TF_PPC) for good. A 68k program that only looks at the card (SysSpeed
*   reports the CPU and has one PPC test) ran its 68k tests from PPC memory, and a Shell or
*   Workbench that had loaded such a program kept giving PPC memory to everything after.
*
*   17.17 DIVERGES FROM 17.13b AND 17.16 here, on purpose:
*
*   1. Hunks are placed by what they hold. The file's hunks are walked before it is
*      loaded (scanHunks: the header, each hunk's type, the words of each code hunk, the
*      data skipped by Seek). The PPC never runs 68k code, so 68k code goes to Amiga
*      memory (FAST); PPC code (HUNK_PPC_CODE, or a HUNK_CODE or HUNK_DATA whose words
*      are PPC instructions: scanCode) goes to PPC memory; DATA and BSS go to PPC memory when the
*      file has PPC code (its PPC code reads them), as before. A file without PPC code is
*      loaded by dos.library unchanged: its 68k code calls powerpc.library at most, and memory it
*      shares with the PPC comes from AllocVec32/AllocVecPPC, or from AllocMem once the
*      process runs PPC code (RunPPC tags it, below).
*   2. The loading process is not tagged by a load. After a program with PPC code it is
*      left as it was found (17.16 left it tagged for good); after a 68k program it is left
*      untagged, as 17.16 did, and dos.library's own load runs untagged (else the AllocMem
*      patch puts the 68k program in PPC memory). A process is tagged when it opens
*      this library while running a PPC program (myOpen asks PPCSegTask), or when it
*      runs PPC code (RunPPC), or allocates with AllocVec32, as before. A Shell gets its
*      state back when the command it ran is unloaded (patchUnLoadSeg).
*   3. Nothing is written to the file. 17.16 set protection bits 16 (PPC) and 17 (not
*      PPC) on every program it loaded, which changed users' files and, on read-only
*      volumes, made every start load the whole program into PPC memory again to scan it.
*      The decisions are kept in memory now (loadCache). Bits 16 and 17 already set by
*      older versions are ignored.
*   4. ENV:sonnet/LoadNormal and ENV:sonnet/LoadLegacy hold lists of AmigaDOS patterns
*      (separated by spaces or new lines), matched without case against the file's name.
*      LoadNormal: dos.library loads it unchanged. LoadLegacy: 17.16's loader, string
*      scan and tagging included, for a program that hands static data to PPC code it
*      did not bring itself. LoadLegacy "#?" gives 17.16's behaviour everywhere. Both are
*      read at every load, so they work without a reboot.
*
*   A file this code does not understand (an overlay, resident library names in its
*   header, a hunk type not listed in scanHunks) goes to 17.16's loader, minus the file
*   writes. ENV:sonnet/DisHunkPatch keeps its meaning: no hunk is moved to FAST by this
*   library. Cybermand and CyberPi keep their exception (no hunk moved).
*
*********************************************************************************************/

#define LS_NORMAL       0       //dos.library loads it unchanged
#define LS_EHF          1       //placed by hunk type
#define LS_LEGACY       2       //17.16's loader, string scan included
#define LS_KEEP         3       //cache only: 17.16's loader found a string, load and keep without the scan

#define LP_NONE         0xFF    //lp_Top: this hunk's size word is not touched

#define SCAN_BUF        2048
#define SCAN_MAXHUNKS   4096
#define SCAN_MAXLONGS   0x01000000  //a count above 64 MB is not a hunk file this code reads

#define ENVLIST_MAX     512

#ifndef HUNK_PPC_CODE
#define HUNK_PPC_CODE   1257    //EHF (vlink amigahunks.h)
#endif
#ifndef HUNK_RELRELOC26
#define HUNK_RELRELOC26 1260    //EHF, long format
#endif

/* one hunk of the file being loaded: where its size word is, the word, its type, and the
   two top bits (MEMF: 1 CHIP, 2 FAST) ReadFunc writes into it */
struct LoadPatch
{
    ULONG lp_Offset;
    ULONG lp_Size;
    ULONG lp_Type;
    ULONG lp_Top;
};

/* a load in progress, found by ReadFunc through its file handle (InternalLoadSeg passes
   nothing else). Claimed and freed under Forbid; one per loading task. */
struct LoadCtx
{
    BPTR              lc_File;
    LONG              lc_Mode;
    ULONG             lc_Pos;       //bytes read so far
    ULONG             lc_Next;      //first patch not yet behind lc_Pos
    ULONG             lc_Count;
    struct LoadPatch* lc_Patches;
};

#define LOADCTX_MAX     8
static struct LoadCtx loadCtx[LOADCTX_MAX];

/* what was decided for a file, by its disk key, size, date and name */
struct LoadCache
{
    ULONG c_Key;
    ULONG c_Size;
    ULONG c_Days;
    ULONG c_Minute;
    ULONG c_Tick;
    ULONG c_Name;
    ULONG c_Mode;
    ULONG c_Used;
};

#define LOADCACHE_MAX   32
static struct LoadCache loadCache[LOADCACHE_MAX];
static ULONG loadCacheNext;

/* a seglist this library loaded: whether it holds PPC code (myOpen), and the TF_PPC state
   of the process that loaded it, given back when a Shell unloads its command */
struct SegTrack
{
    BPTR         st_Seg;
    struct Task* st_Task;
    ULONG        st_Had;
    ULONG        st_PPC;
    ULONG        st_Age;
};

#define SEGTRACK_MAX    32
static struct SegTrack segTrack[SEGTRACK_MAX];
static ULONG segTrackAge;

/********************************************************************************************/

/* Amiga memory through exec's own AllocMem: the patch above could send it to PPC memory */
static APTR plainAlloc(ULONG size, struct ExecBase* SysBase)
{
    APTR (*AllocMem_ptr)(__reg("d0") ULONG, __reg("d1") ULONG, __reg("a6") struct ExecBase*) = OldAllocMem;

    return ((*AllocMem_ptr)(size, MEMF_PUBLIC | MEMF_CLEAR, SysBase));
}

static ULONG nameHash(CONST_STRPTR name)
{
    ULONG hash = 5381;
    UBYTE c;

    while (c = *name++)
    {
        if ((c >= 'a') && (c <= 'z'))
        {
            c -= 0x20;
        }
        hash = (hash * 33) ^ c;
    }
    return hash;
}

static LONG cacheFind(struct LoadCache* key, struct ExecBase* SysBase)
{
    LONG mode = -1;

    Forbid();

    for (int i = 0; i < LOADCACHE_MAX; i++)
    {
        struct LoadCache* c = &loadCache[i];

        if ((c->c_Used) && (c->c_Key == key->c_Key) && (c->c_Size == key->c_Size) && (c->c_Days == key->c_Days)
            && (c->c_Minute == key->c_Minute) && (c->c_Tick == key->c_Tick) && (c->c_Name == key->c_Name))
        {
            mode = c->c_Mode;
            break;
        }
    }

    Permit();

    return mode;
}

static void cacheAdd(struct LoadCache* key, ULONG mode, struct ExecBase* SysBase)
{
    Forbid();

    struct LoadCache* c = &loadCache[loadCacheNext];

    loadCacheNext = (loadCacheNext + 1) % LOADCACHE_MAX;
    *c = *key;
    c->c_Mode = mode;
    c->c_Used = 1;

    Permit();
}

/********************************************************************************************/

static struct LoadCtx* ctxClaim(BPTR file, LONG mode, struct LoadPatch* patches, ULONG count, struct ExecBase* SysBase)
{
    struct LoadCtx* ctx = NULL;

    Forbid();

    for (int i = 0; i < LOADCTX_MAX; i++)
    {
        if (!(loadCtx[i].lc_File))
        {
            ctx = &loadCtx[i];
            ctx->lc_File    = file;
            ctx->lc_Mode    = mode;
            ctx->lc_Pos     = 0;
            ctx->lc_Next    = 0;
            ctx->lc_Count   = count;
            ctx->lc_Patches = patches;
            break;
        }
    }

    Permit();

    return ctx;
}

static void ctxRelease(struct LoadCtx* ctx, struct ExecBase* SysBase)
{
    if (ctx)
    {
        Forbid();
        ctx->lc_File = 0;
        Permit();
    }
}

static struct LoadCtx* ctxFind(BPTR file)
{
    for (int i = 0; i < LOADCTX_MAX; i++)
    {
        if (loadCtx[i].lc_File == file)
        {
            return &loadCtx[i];
        }
    }
    return NULL;
}

/********************************************************************************************/

static void segTrackAdd(BPTR seg, struct Task* task, ULONG had, ULONG ppc, struct ExecBase* SysBase)
{
    Forbid();

    struct SegTrack* slot = &segTrack[0];

    for (int i = 0; i < SEGTRACK_MAX; i++)
    {
        if (!(segTrack[i].st_Seg))
        {
            slot = &segTrack[i];
            break;
        }
        if (segTrack[i].st_Age < slot->st_Age)
        {
            slot = &segTrack[i];
        }
    }
    slot->st_Seg  = seg;
    slot->st_Task = task;
    slot->st_Had  = had;
    slot->st_PPC  = ppc;
    slot->st_Age  = ++segTrackAge;

    Permit();
}

static BOOL isPPCSeg(BPTR seg)
{
    if (seg)
    {
        for (int i = 0; i < SEGTRACK_MAX; i++)
        {
            if ((segTrack[i].st_Seg == seg) && (segTrack[i].st_PPC))
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}

/*
 * Is this task running a program with PPC code (or one loaded by 17.16's loader)? Asked by
 * myOpen. A plain task (not a process) cannot have been loaded: it is tagged as before.
 * A Shell command's seglist is cli_Module; a process started with NP_Seglist (Workbench)
 * has it in pr_SegList[3].
 */
BOOL PPCSegTask(struct Task* task)
{
    struct ExecBase* SysBase = myPPCBase->PPC_SysLib;
    struct Process* proc = (struct Process*)task;
    BOOL found;
    BPTR seg1 = 0;
    BPTR seg2 = 0;

    if (task->tc_Node.ln_Type != NT_PROCESS)
    {
        return TRUE;
    }

    struct CommandLineInterface* cli = (struct CommandLineInterface*)((ULONG)(proc->pr_CLI << 2));

    if (cli)
    {
        seg1 = cli->cli_Module;
    }
    if (proc->pr_SegList)
    {
        LONG* segArray = (LONG*)((ULONG)(proc->pr_SegList << 2));

        if (segArray[0] >= 3)
        {
            seg2 = segArray[3];
        }
    }

    Forbid();
    found = isPPCSeg(seg1) || isPPCSeg(seg2);
    Permit();

    return found;
}

/********************************************************************************************/

/* is a pattern in ENV:<var> (a list of AmigaDOS patterns) matching this file name? */
static BOOL envMatch(CONST_STRPTR var, CONST_STRPTR file, struct ExecBase* SysBase, struct DosLibrary* DOSBase)
{
    BOOL hit = FALSE;
    UBYTE* buffer;
    LONG len;

    if (!(buffer = (UBYTE*)plainAlloc((3 * ENVLIST_MAX) + 2, SysBase)))
    {
        return FALSE;
    }

    len = GetVar(var, (STRPTR)buffer, ENVLIST_MAX, GVF_GLOBAL_ONLY | GVF_BINARY_VAR);

    if (len > 0)
    {
        UBYTE* token = buffer + ENVLIST_MAX;    //2 * ENVLIST_MAX + 2 for ParsePatternNoCase
        UBYTE* p = buffer;

        buffer[(len < ENVLIST_MAX) ? len : (ENVLIST_MAX - 1)] = 0;

        while ((*p) && (!(hit)))
        {
            UBYTE* start;
            UBYTE save;

            while ((*p == ' ') || (*p == '\t') || (*p == '\n') || (*p == '\r'))
            {
                p++;
            }
            if (!(*p))
            {
                break;
            }
            start = p;
            while ((*p) && (*p != ' ') && (*p != '\t') && (*p != '\n') && (*p != '\r'))
            {
                p++;
            }
            save = *p;
            *p = 0;
            if (ParsePatternNoCase((STRPTR)start, (STRPTR)token, (2 * ENVLIST_MAX) + 2) >= 0)
            {
                hit = MatchPatternNoCase((STRPTR)token, (STRPTR)file);
            }
            *p = save;
        }
    }

    FreeMem(buffer, (3 * ENVLIST_MAX) + 2);

    return hit;
}

/********************************************************************************************/

/* the hunk walk reads through a buffer of its own and skips bodies with Seek */
struct HunkScan
{
    BPTR   hs_File;
    UBYTE* hs_Buf;
    LONG   hs_Len;
    LONG   hs_At;
    ULONG  hs_Start;    //file offset of hs_Buf[0]
    LONG   hs_Bad;
};

static ULONG scanByte(struct HunkScan* s, struct DosLibrary* DOSBase)
{
    if (s->hs_Bad)
    {
        return 0;
    }
    if (s->hs_At >= s->hs_Len)
    {
        s->hs_Start += s->hs_Len;
        s->hs_At     = 0;
        s->hs_Len    = Read(s->hs_File, s->hs_Buf, SCAN_BUF);
        if (s->hs_Len <= 0)
        {
            s->hs_Len = 0;
            s->hs_Bad = 1;
            return 0;
        }
    }
    return s->hs_Buf[s->hs_At++];
}

static ULONG scanWord(struct HunkScan* s, struct DosLibrary* DOSBase)
{
    ULONG value = scanByte(s, DOSBase) << 8;

    return (value | scanByte(s, DOSBase));
}

static ULONG scanLong(struct HunkScan* s, struct DosLibrary* DOSBase)
{
    ULONG value = scanWord(s, DOSBase) << 16;

    return (value | scanWord(s, DOSBase));
}

static ULONG scanPos(struct HunkScan* s)
{
    return (s->hs_Start + s->hs_At);
}

static void scanSkip(struct HunkScan* s, ULONG bytes, struct DosLibrary* DOSBase)
{
    if (s->hs_Bad)
    {
        return;
    }
    if (bytes <= (ULONG)(s->hs_Len - s->hs_At))
    {
        s->hs_At += bytes;
        return;
    }

    ULONG target = scanPos(s) + bytes;

    if (Seek(s->hs_File, target, OFFSET_BEGINNING) < 0)
    {
        s->hs_Bad = 1;
        return;
    }
    s->hs_Start = target;
    s->hs_Len   = 0;
    s->hs_At    = 0;
}

static ULONG scanCount(struct HunkScan* s, struct DosLibrary* DOSBase)
{
    ULONG count = scanLong(s, DOSBase);

    if (count > SCAN_MAXLONGS)
    {
        s->hs_Bad = 1;
        return 0;
    }
    return count;
}

/*
 * PPC CODE IS TOLD BY ITS CONTENT, NOT ONLY BY ITS TYPE. vlink writes HUNK_PPC_CODE only
 * for sections it has marked as PPC; WarpOS programs built by vbcc carry their PPC code
 * in plain HUNK_CODE (WarpQuake2PPC, SysSpeed's PPC test hunk, rtgmaster). So a code
 * hunk's words are counted: aligned blr, mflr r0, mtlr r0 and stwu r1,-n(r1) for PPC;
 * rts, link a5, unlk a5, movem.l to and from the stack (as halfwords) for 68k. A hunk is
 * PPC code with PPC marks and no 68k ones, or with at least one PPC mark for every eight
 * 68k ones. The rule leans to PPC on purpose: 68k code in PPC memory only runs slowly
 * (what 17.16 did to every hunk but the first), PPC code in Amiga memory does not run.
 * Data hunks are counted too, with a higher bar (four PPC marks): Elf2Exe2, which makes
 * MiniGL's minigl_ppc.dll, puts the PPC code in HUNK_DATA (379,736 bytes, 3,777 marks).
 * Checked over 5,536 hunk files of a WinUAE system (2026-11-15): no 68k program is taken
 * for PPC code (no 68k data hunk has more than one PPC mark); the PPC hunks found are
 * SysSpeed's, rtgmaster's and WarpUp tools'.
 */
static void scanCode(struct HunkScan* s, ULONG longs, ULONG* ppcOut, ULONG* m68kOut, struct DosLibrary* DOSBase)
{
    ULONG ppc = 0;
    ULONG m68k = 0;

    while ((longs) && (!(s->hs_Bad)))
    {
        ULONG word;

        if ((s->hs_Len - s->hs_At) >= 4)
        {
            UBYTE* b = &s->hs_Buf[s->hs_At];

            word = ((ULONG)b[0] << 24) | ((ULONG)b[1] << 16) | ((ULONG)b[2] << 8) | b[3];
            s->hs_At += 4;
        }
        else
        {
            word = scanLong(s, DOSBase);
        }

        if ((word == 0x4E800020) || (word == 0x7C0802A6) || (word == 0x7C0803A6) || ((word & 0xFFFF8000) == 0x94218000))
        {
            ppc += 1;
        }

        for (int half = 0; half < 2; half++)
        {
            ULONG h = half ? (word & 0xFFFF) : (word >> 16);

            if ((h == 0x4E75) || (h == 0x4E55) || (h == 0x4E5D) || (h == 0x48E7) || (h == 0x4CDF))
            {
                m68k += 1;
            }
        }
        longs -= 1;
    }
    *ppcOut  = ppc;
    *m68kOut = m68k;
}

/*
 * Walks a load file from its start: LS_NORMAL when it has no PPC code, LS_EHF with the
 * patch list made when it has, LS_LEGACY when it is not a file this walk understands.
 * The handle is left anywhere; the caller seeks back to the start.
 */
static LONG scanHunks(BPTR file, struct LoadPatch** patchesOut, ULONG* countOut, ULONG* bytesOut,
                      struct ExecBase* SysBase, struct DosLibrary* DOSBase)
{
    struct HunkScan scan;
    struct HunkScan* s = &scan;
    struct LoadPatch* patches = NULL;
    ULONG count = 0;
    ULONG bytes = 0;
    ULONG hasPPC = 0;
    ULONG i;
    LONG result = LS_LEGACY;

    *patchesOut = NULL;
    *countOut   = 0;
    *bytesOut   = 0;

    s->hs_File  = file;
    s->hs_Len   = 0;
    s->hs_At    = 0;
    s->hs_Start = 0;
    s->hs_Bad   = 0;

    if (!(s->hs_Buf = (UBYTE*)plainAlloc(SCAN_BUF, SysBase)))
    {
        return LS_LEGACY;
    }

    if ((scanLong(s, DOSBase) != HUNK_HEADER) || (scanLong(s, DOSBase) != 0))   //no resident library names in a program
    {
        goto done;
    }

    count = scanLong(s, DOSBase);

    if ((scanLong(s, DOSBase) != 0) || (scanLong(s, DOSBase) != (count - 1)) || (!(count)) || (count > SCAN_MAXHUNKS)
        || (s->hs_Bad))
    {
        goto done;
    }

    bytes = count * sizeof(struct LoadPatch);

    if (!(patches = (struct LoadPatch*)plainAlloc(bytes, SysBase)))
    {
        goto done;
    }

    for (i = 0; i < count; i++)
    {
        patches[i].lp_Offset = scanPos(s);
        patches[i].lp_Size   = scanLong(s, DOSBase);
        patches[i].lp_Top    = LP_NONE;
        if ((patches[i].lp_Size & 0xC0000000) == 0xC0000000)
        {
            scanLong(s, DOSBase);                   //extended memory attributes follow
        }
    }

    i = 0;

    while ((i < count) && (!(s->hs_Bad)))
    {
        ULONG type = scanLong(s, DOSBase) & 0x3FFFFFFF;
        ULONG n;

        if (s->hs_Bad)
        {
            break;
        }

        switch (type)
        {
            case HUNK_NAME:
            case HUNK_DEBUG:
            {
                n = scanCount(s, DOSBase);
                scanSkip(s, n * 4, DOSBase);
                break;
            }
            case HUNK_CODE:
            case HUNK_PPC_CODE:
            {
                ULONG ppcMarks = 0;
                ULONG m68kMarks = 0;

                n = scanCount(s, DOSBase);
                scanCode(s, n, &ppcMarks, &m68kMarks, DOSBase);
                if ((type == HUNK_PPC_CODE) || ((ppcMarks) && (!(m68kMarks))) || ((ppcMarks >= 2) && ((ppcMarks * 8) >= m68kMarks)))
                {
                    patches[i].lp_Type = HUNK_PPC_CODE;
                    hasPPC = 1;
                }
                else
                {
                    patches[i].lp_Type = HUNK_CODE;
                }
                break;
            }
            case HUNK_DATA:
            {
                ULONG ppcMarks = 0;
                ULONG m68kMarks = 0;

                n = scanCount(s, DOSBase);
                scanCode(s, n, &ppcMarks, &m68kMarks, DOSBase);
                if ((ppcMarks >= 4) && ((ppcMarks * 8) >= m68kMarks))
                {
                    patches[i].lp_Type = HUNK_PPC_CODE;     //PPC code in a data hunk (Elf2Exe2's DLLs)
                    hasPPC = 1;
                }
                else
                {
                    patches[i].lp_Type = type;
                }
                break;
            }
            case HUNK_BSS:
            {
                scanLong(s, DOSBase);
                patches[i].lp_Type = type;
                break;
            }
            case HUNK_RELOC32:
            case HUNK_RELOC16:
            case HUNK_RELOC8:
            case HUNK_DREL16:
            case HUNK_DREL8:
            case HUNK_RELRELOC26:
            {
                while ((n = scanCount(s, DOSBase)) && (!(s->hs_Bad)))
                {
                    scanSkip(s, 4 + (n * 4), DOSBase);
                }
                break;
            }
            case HUNK_RELOC32SHORT:
            case HUNK_DREL32:                       //in a load file the short format (doshunks.h)
            {
                while ((n = scanWord(s, DOSBase)) && (!(s->hs_Bad)))
                {
                    scanSkip(s, 2 + (n * 2), DOSBase);
                }
                if (scanPos(s) & 2)
                {
                    scanSkip(s, 2, DOSBase);
                }
                break;
            }
            case HUNK_SYMBOL:
            {
                while ((n = scanLong(s, DOSBase)) && (!(s->hs_Bad)))
                {
                    scanSkip(s, ((n & 0x00FFFFFF) * 4) + 4, DOSBase);
                }
                break;
            }
            case HUNK_END:
            {
                if (!(patches[i].lp_Type))
                {
                    s->hs_Bad = 1;
                }
                i++;
                break;
            }
            default:
            {
                s->hs_Bad = 1;                      //an overlay, a break, a type not known here
                break;
            }
        }
    }

    if ((s->hs_Bad) || (i < count))
    {
        goto done;
    }

    if ((scanLong(s, DOSBase) & 0x3FFFFFFF) == HUNK_OVERLAY)
    {
        if (!(s->hs_Bad))
        {
            goto done;                              //the root of an overlaid file: 17.16's loader
        }
    }

    if (!(hasPPC))
    {
        result = LS_NORMAL;
        goto done;
    }

    result = LS_EHF;

    BOOL noPatch = (myPPCBase->PPC_Flags & LIB_FLAGPPC_DHP) ? TRUE : FALSE;

    if (count >= 2)
    {
        if (((patches[0].lp_Size == 0x71E) && (patches[1].lp_Size == 0x710)) ||    //Cybermand
            ((patches[0].lp_Size == 0x84E) && (patches[1].lp_Size == 0xEE)))       //CyberPi
        {
            noPatch = TRUE;
        }
    }

    if (!(noPatch))
    {
        for (i = 0; i < count; i++)
        {
            ULONG top = patches[i].lp_Size >> 30;

            if (top == 3)
            {
                continue;                           //extended attributes: as the file asks
            }
            if ((patches[i].lp_Type == HUNK_CODE) && (top != 1))
            {
                patches[i].lp_Top = 2;              //68k code: FAST (a CHIP request is kept)
            }
            else if ((patches[i].lp_Type == HUNK_PPC_CODE) && (top != 0))
            {
                patches[i].lp_Top = 0;              //PPC code: never Amiga memory
            }
        }
    }

done:
    FreeMem(s->hs_Buf, SCAN_BUF);

    if ((result == LS_EHF) && (patches))
    {
        *patchesOut = patches;
        *countOut   = count;
        *bytesOut   = bytes;
    }
    else if (patches)
    {
        FreeMem(patches, bytes);
    }
    return result;
}

/********************************************************************************************/

LONG ReadFunc(__reg("d1") BPTR readhandle, __reg("d2") APTR buffer, __reg("d3") LONG length, __reg("a6") struct DosLibrary* DOSBase)
{
    LONG result;

    ULONG* myBuffer = (ULONG*)buffer;
    struct LoadCtx* ctx = ctxFind(readhandle);

    result = Read(readhandle, buffer, length);

    if ((ctx) && (ctx->lc_Mode == LS_EHF))
    {
        /*
         * The size words of the hunks to move, by their offset in the file: only the top
         * byte changes (bits 31 and 30, FAST and CHIP), so a word split between two reads
         * needs no care.
         */
        if (result > 0)
        {
            ULONG start = ctx->lc_Pos;
            ULONG end   = start + result;
            struct LoadPatch* patches = ctx->lc_Patches;

            while ((ctx->lc_Next < ctx->lc_Count) && (patches[ctx->lc_Next].lp_Offset < start))
            {
                ctx->lc_Next += 1;
            }
            for (ULONG n = ctx->lc_Next; (n < ctx->lc_Count) && (patches[n].lp_Offset < end); n++)
            {
                if (patches[n].lp_Top != LP_NONE)
                {
                    UBYTE* top = (UBYTE*)buffer + (patches[n].lp_Offset - start);

                    *top = (*top & 0x3F) | (UBYTE)(patches[n].lp_Top << 6);
                }
            }
            ctx->lc_Pos = end;
        }
        return result;
    }

    if (!(myPPCBase->PPC_Flags & LIB_FLAGPPC_DHP))
    {
        if ((myBuffer[0] == HUNK_HEADER) && (myBuffer[1] == NULL) && (myBuffer[3] == NULL) && (myBuffer[2] - myBuffer[4] - 1 == NULL))
        {
            /*
             * 17.16's rule, for its loader (LS_LEGACY): the first hunk to FAST.
             * THE TWO EXCEPTIONS 17.13b HAS (powerpc.s:3506-3522): programs
             * whose first hunk is not marked FAST, so they load entirely into
             * PPC memory. 17.15 added a third, the hunk sizes of one build of
             * the Radeon driver (0x9468, 0x4F3); removed on the user's word,
             * 4 Nov 2026. WarpOS has no loader patch at all.
             */
            if (((myBuffer[5] == 0x71E) && (myBuffer[6] == 0x710)) ||  //Cybermand
                ((myBuffer[5] == 0x84E) && (myBuffer[6] == 0xEE)))     //CyberPi
            {
                return result;
            }
            else if (!(myBuffer[5] & (1<<HUNKB_CHIP)))
            {
                myBuffer[5] |= (1<<HUNKB_FAST);
            }
        }
    }
    return result;
}

/********************************************************************************************/

APTR AllocFunc(__reg("d0") ULONG size, __reg("d1") ULONG flags, __reg("a6") struct ExecBase* SysBase)
{
    APTR memBlock;

    /*
     * A hunk asking for exactly FAST or CHIP (with PUBLIC) gets Amiga
     * memory, every other hunk PPC memory. This used to be skipped when
     * PPC_Flags bit 0x4 -- LIB_FLAGPPC_SCC, ENV:sonnet/SetCPUComm -- was set,
     * which sent every hunk to PPC memory. That test was a mistake (the
     * user, 31 Oct 2026): 17.13b made this choice on DisHunkPatch
     * (powerpc.s:3526-3528), and SetCPUComm is about cross-CPU signals,
     * which functionsppc.c still handles. Removed; this is what an unset
     * SetCPUComm always did.
     */
    if ((flags == (MEMF_PUBLIC | MEMF_FAST)) || (flags == (MEMF_PUBLIC | MEMF_CHIP)))
    {
        SysBase->ThisTask->tc_Flags &= ~TF_PPC;
        memBlock = AllocMem(size, flags);
        SysBase->ThisTask->tc_Flags |= TF_PPC;
        return memBlock;
    }
    SysBase->ThisTask->tc_Flags |= TF_PPC;
    flags = (flags & (MEMF_REVERSE | MEMF_CLEAR)) | MEMF_PUBLIC | MEMF_PPC;
    memBlock = AllocMem(size, flags);
    return memBlock;
}

void FreeFunc(__reg("a1") APTR memory, __reg("d0") ULONG size, __reg("a6") struct ExecBase* SysBase)
{
    FreeMem(memory, size);

    return;
}

/********************************************************************************************/

/*
 * 17.16's loader for one open file (LS_LEGACY, LS_KEEP): every hunk but the first in PPC
 * memory, kept when a string marks it as PPC related. Returns the seglist, or -1 for
 * dos.library to load the file (the handle is closed either way). Sets *keptOut when the
 * load is kept; TF_PPC is then left set, as 17.16 left it.
 */
static BPTR legacyLoad(STRPTR name, BPTR myLock, LONG mode, struct LoadCache* key, BOOL useCache, ULONG* keptOut,
                       struct ExecBase* SysBase, struct DosLibrary* DOSBase)
{
    struct FuncTable
    {
        APTR ReadFunc;
        APTR AllocFunc;
        APTR FreeFunc;
        ULONG Stack;
    };

    struct FuncTable myFuncTable;
    struct LoadCtx* ctx;
    BPTR mySeglist;

    *keptOut = 0;

    ctx = ctxClaim(myLock, LS_LEGACY, NULL, 0, SysBase);

    SysBase->ThisTask->tc_Flags |= TF_PPC;
    myFuncTable.ReadFunc  = (APTR)&ReadFunc;
    myFuncTable.AllocFunc = (APTR)&AllocFunc;
    myFuncTable.FreeFunc  = (APTR)&FreeFunc;
    myFuncTable.Stack     = 0;
    mySeglist = InternalLoadSeg(myLock, NULL, (LONG*)&myFuncTable, (LONG*)&myFuncTable.Stack);

    ctxRelease(ctx, SysBase);

    if (mySeglist < 0)
    {
        /*
         * AN OVERLAID FILE (P4, REVIEW-LIB.md, 10 Nov 2026).
         * InternalLoadSeg returns -(seglist) and keeps the file
         * open for the overlay manager; unloading the seglist is
         * what closes it (NDK 3.9 dos.doc:2803-2804, :2845-2846,
         * :5423-5424). So no Close here, and it is the seglist
         * that is unloaded, not !mySeglist (which is 0), as in
         * 17.13b (powerpc.s:3414-3416, :3469).
         */
        UnLoadSeg(-mySeglist);
        if (useCache)
        {
            cacheAdd(key, LS_NORMAL, SysBase);
        }
        return -1;
    }

    Close(myLock);

    if (!(mySeglist))
    {
        return NULL;
    }

    if (mode == LS_KEEP)
    {
        *keptOut = 1;
        return mySeglist;
    }

    BPTR testSeglist = mySeglist;

    while (testSeglist)
    {
        ULONG curPointer = (ULONG)testSeglist << 2;
        UBYTE* mySegData = (UBYTE*)(curPointer + 4);
        LONG mySegSize   = (*((ULONG*)(curPointer - 4)));
        testSeglist = (*((ULONG*)(curPointer)));

        for (int i=mySegSize; i >= 0; i--)
        {
            ULONG* myLongData = (ULONG*)mySegData;
            BOOL found = FALSE;

            if (myLongData[0] == 0x52414345)            // "RACE"
            {
                found = TRUE;
            }
            for (int i=0; (i<20) && (!(found)); i++)
            {
                if (mySegData[i] == powerlib[i])
                {
                    if (powerlib[i+1] == 0)
                    {
                        found = TRUE;
                    }
                }
                else
                {
                    break;
                }
            }
            for (int i=0; (i<20) && (!(found)); i++)
            {
                if (mySegData[i] == ppclib[i])
                {
                    if (ppclib[i+1] == 0)
                    {
                        found = TRUE;
                    }
                }
                else
                {
                    break;
                }
            }
            for (int i=0; (i<20) && (!(found)); i++)
            {
                if (mySegData[i] == ampname[i])
                {
                    if (ampname[i+1] == 0)
                    {
                        found = TRUE;
                    }
                }
                else
                {
                    break;
                }
            }
            if (found)
            {
                if (useCache)
                {
                    cacheAdd(key, LS_KEEP, SysBase);
                }
                *keptOut = 1;
                return mySeglist;
            }
            mySegData += 1;
        }
    }

    UnLoadSeg(mySeglist);
    if (useCache)
    {
        cacheAdd(key, LS_NORMAL, SysBase);
    }
    return -1;
}

/********************************************************************************************/

/*
 * Returns the seglist, 0 for a failure, or -1 for dos.library to load the file. TF_PPC is
 * left CLEAR on every return but a kept 17.16 load (*keepOut): dos.library's own load must
 * run untagged, or the AllocMem patch sends its hunks to PPC memory -- a 68k library that
 * ramlib loads after something tagged ramlib went to the card's memory (MiniGL's 68k
 * library: Quake II GL 19.8 -> 21.2 ms a frame, found 2026-11-15). After a program with
 * PPC code the caller gives the task its state back (patchLoadSeg); after a 68k program
 * it stays untagged, as 17.16 left it, so that ramlib's InitResident (MakeLibrary: the
 * library's base and jump table) runs untagged too.
 */
BPTR myLoader(STRPTR name, ULONG* ppcOut, ULONG* keepOut) // -1 = try normal 0 = fail
{
    struct FuncTable
    {
        APTR ReadFunc;
        APTR AllocFunc;
        APTR FreeFunc;
        ULONG Stack;
    };

    struct ExecBase* SysBase   = myPPCBase->PPC_SysLib;
    struct DosLibrary* DOSBase = myPPCBase->PPC_DosLib;
    struct Task* myTask        = SysBase->ThisTask;
    struct FileInfoBlock* myFIB;
    struct LoadCache key;
    BPTR myLock;
    BPTR result = -1;
    BOOL useCache = FALSE;
    LONG mode;

    *ppcOut  = 0;
    *keepOut = 0;

    if ((myTask->tc_Node.ln_Name) && (*((ULONG*)myTask->tc_Node.ln_Name) == 0x44656649))    //DefI(cons)
    {
        return -1;
    }

    myTask->tc_Flags &= ~TF_PPC;

    if (!(myLock = Open(name, MODE_OLDFILE)))
    {
        goto out;
    }

    if (!(myFIB = (struct FileInfoBlock*)AllocDosObject(DOS_FIB, NULL)))
    {
        /* AllocDosObject failed: 17.13b closes the handle here too (powerpc.s:3369, :3406) (P4) */
        Close(myLock);
        goto out;
    }

    if (!(ExamineFH(myLock, myFIB)))
    {
        /* ExamineFH failed (a handler that cannot examine a handle): free the FIB and the handle (P4) */
        FreeDosObject(DOS_FIB, (APTR)myFIB);
        Close(myLock);
        goto out;
    }

    key.c_Key    = myFIB->fib_DiskKey;
    key.c_Size   = myFIB->fib_Size;
    key.c_Days   = myFIB->fib_Date.ds_Days;
    key.c_Minute = myFIB->fib_Date.ds_Minute;
    key.c_Tick   = myFIB->fib_Date.ds_Tick;
    key.c_Name   = nameHash(FilePart(name));
    key.c_Mode   = 0;
    key.c_Used   = 0;

    FreeDosObject(DOS_FIB, (APTR)myFIB);

    struct LoadPatch* patches = NULL;
    ULONG patchCount = 0;
    ULONG patchBytes = 0;

    if (envMatch("sonnet/LoadNormal", FilePart(name), SysBase, DOSBase))
    {
        mode = LS_NORMAL;
    }
    else if (envMatch("sonnet/LoadLegacy", FilePart(name), SysBase, DOSBase))
    {
        mode = LS_LEGACY;
    }
    else
    {
        useCache = TRUE;

        if ((mode = cacheFind(&key, SysBase)) < 0)
        {
            mode = scanHunks(myLock, &patches, &patchCount, &patchBytes, SysBase, DOSBase);

            if (Seek(myLock, 0, OFFSET_BEGINNING) < 0)
            {
                if (patches)
                {
                    FreeMem(patches, patchBytes);
                }
                Close(myLock);
                goto out;
            }
            if (mode == LS_NORMAL)
            {
                cacheAdd(&key, LS_NORMAL, SysBase);
            }
        }
        else if (mode == LS_EHF)
        {
            mode = LS_LEGACY;       //(never cached: an EHF is walked at every load for its patch list)
        }
    }

    if (mode == LS_NORMAL)
    {
        Close(myLock);
        goto out;
    }

    if (mode == LS_EHF)
    {
        struct FuncTable myFuncTable;
        struct LoadCtx* ctx = ctxClaim(myLock, LS_EHF, patches, patchCount, SysBase);
        BPTR mySeglist;

        myTask->tc_Flags |= TF_PPC;
        myFuncTable.ReadFunc  = (APTR)&ReadFunc;
        myFuncTable.AllocFunc = (APTR)&AllocFunc;
        myFuncTable.FreeFunc  = (APTR)&FreeFunc;
        myFuncTable.Stack     = 0;
        mySeglist = InternalLoadSeg(myLock, NULL, (LONG*)&myFuncTable, (LONG*)&myFuncTable.Stack);

        ctxRelease(ctx, SysBase);

        if (patches)
        {
            FreeMem(patches, patchBytes);
        }

        if (mySeglist < 0)
        {
            UnLoadSeg(-mySeglist);  //an overlay after all (scanHunks refuses them): unloading closes the file
            goto out;
        }

        Close(myLock);
        result = mySeglist;
        if (mySeglist)
        {
            *ppcOut = 1;
        }
        goto out;
    }

    {
        ULONG kept;

        result = legacyLoad(name, myLock, mode, &key, useCache, &kept, SysBase, DOSBase);

        if (kept)
        {
            *ppcOut  = 1;
            *keepOut = 1;
            return result;          //TF_PPC stays set, as 17.16 left it
        }
    }

out:
    myTask->tc_Flags &= ~TF_PPC;
    return result;
}

/********************************************************************************************/

/* the loading process's TF_PPC before the load, and the seglist's kind, for patchUnLoadSeg and myOpen */
static void trackLoad(BPTR seg, struct Task* task, ULONG had, ULONG ppc, struct ExecBase* SysBase)
{
    if (seg)
    {
        struct Process* proc = (struct Process*)task;

        if ((ppc) || ((task->tc_Node.ln_Type == NT_PROCESS) && (proc->pr_CLI)))
        {
            segTrackAdd(seg, task, had, ppc, SysBase);
        }
    }
}

PATCH68K BPTR patchLoadSeg(__reg("d1") STRPTR name, __reg("a6") struct DosLibrary* DOSBase)
{
    BPTR (*LoadSeg_ptr)(__reg("d1") STRPTR, __reg("a6") struct DosLibrary*) = OldLoadSeg;
    struct ExecBase* SysBase = myPPCBase->PPC_SysLib;
    struct Task* myTask = SysBase->ThisTask;
    ULONG hadPPC = myTask->tc_Flags & TF_PPC;
    ULONG ppc;
    ULONG keep;

    BPTR mySegList;

    mySegList = myLoader(name, &ppc, &keep);

    if (mySegList == -1)
    {
        mySegList = ((*LoadSeg_ptr)(name, DOSBase));    //untagged: a 68k file's hunks in Amiga memory
    }
    if ((ppc) && (!(keep)))
    {
        myTask->tc_Flags = (myTask->tc_Flags & ~TF_PPC) | hadPPC;
    }
    trackLoad(mySegList, myTask, hadPPC, ppc, SysBase);
    return mySegList;
}

/********************************************************************************************/

PATCH68K BPTR patchNewLoadSeg(__reg("d1") STRPTR file, __reg("d2") struct TagItem* tags,
                     __reg("a6") struct DosLibrary* DOSBase)
{
    BPTR (*NewLoadSeg_ptr)(__reg("d1") STRPTR, __reg("d2") struct TagItem*, __reg("a6") struct DosLibrary*) = OldNewLoadSeg;
    struct ExecBase* SysBase = myPPCBase->PPC_SysLib;
    struct Task* myTask = SysBase->ThisTask;
    ULONG hadPPC = myTask->tc_Flags & TF_PPC;
    ULONG ppc = 0;
    ULONG keep = 0;

    BPTR mySegList;
    if (tags)
    {
        mySegList = ((*NewLoadSeg_ptr)(file, tags, DOSBase));
        trackLoad(mySegList, myTask, hadPPC, 0, SysBase);
        return mySegList;
    }

    mySegList = myLoader(file, &ppc, &keep);

    if (mySegList == -1)
    {
        mySegList = ((*NewLoadSeg_ptr)(file, tags, DOSBase));   //untagged, as patchLoadSeg
    }
    if ((ppc) && (!(keep)))
    {
        myTask->tc_Flags = (myTask->tc_Flags & ~TF_PPC) | hadPPC;
    }
    trackLoad(mySegList, myTask, hadPPC, ppc, SysBase);
    return mySegList;
}

/********************************************************************************************/

/*
 * 17.17: a Shell gets its TF_PPC back when the command it loaded is unloaded. The command
 * ran in the Shell's own process, so anything that tagged it then (this library's
 * OpenLibrary, RunPPC, AllocVec32, a kept 17.16 load) would otherwise stay for every
 * command after. Only the process that loaded the seglist, only a CLI process unloading
 * its own command (cli_Module, or 0 when the Shell has cleared it first), and only when
 * it was not tagged before the load: a program unloading a plugin of its own is left
 * alone.
 */
PATCH68K BOOL patchUnLoadSeg(__reg("d1") BPTR seglist, __reg("a6") struct DosLibrary* DOSBase)
{
    BOOL (*UnLoadSeg_ptr)(__reg("d1") BPTR, __reg("a6") struct DosLibrary*) = OldUnLoadSeg;

    if (seglist)
    {
        struct ExecBase* SysBase = myPPCBase->PPC_SysLib;
        struct Task* myTask = SysBase->ThisTask;

        Forbid();

        for (int i = 0; i < SEGTRACK_MAX; i++)
        {
            if (segTrack[i].st_Seg == seglist)
            {
                if ((segTrack[i].st_Task == myTask) && (!(segTrack[i].st_Had)) && (myTask->tc_Node.ln_Type == NT_PROCESS))
                {
                    struct CommandLineInterface* cli = (struct CommandLineInterface*)((ULONG)(((struct Process*)myTask)->pr_CLI << 2));

                    if ((cli) && ((cli->cli_Module == seglist) || (!(cli->cli_Module))))
                    {
                        myTask->tc_Flags &= ~TF_PPC;
                    }
                }
                segTrack[i].st_Seg = 0;
                break;
            }
        }

        Permit();
    }

    return ((*UnLoadSeg_ptr)(seglist, DOSBase));
}

/********************************************************************************************
*
*	Process that handles messages from the PPC.
*
*********************************************************************************************/

FUNC68K void MirrorTask(void)
{
	struct PrivatePPCBase* PowerPCBase = (struct PrivatePPCBase*)myPPCBase;
	struct ExecBase* SysBase = PowerPCBase->pp_PowerPCBase.PPC_SysLib;
	struct MsgFrame* myFrame;
    struct MsgPort* mirrorPort;
    struct MirrorTask* myMirror = NULL;

    struct Task* myTask = SysBase->ThisTask;

    /*
     * THE DIRECTORY LOCK DOS GAVE THIS PROCESS, put back before every return.
     * DOS unlocks pr_CurrentDir when a CreateNewProc process ends. A PPC task
     * may CurrentDir() a lock it does not own onto its mirror -- iFUSION's
     * poll task sets the Shell's own lock, emulate.c:1233, and never DupLocks
     * it -- and that lock would be freed with the mirror: after one run the
     * Shell reported iFUSION "not an executable" until a reset. The
     * original's mirrors come from CreateProc (powerpc.s:2162), whose exit
     * unlocks nothing, which is why the original leaves the Shell intact.
     * FIXES.md "THE MIRROR'S DIRECTORY LOCK", 23 Oct 2026.
     */
    BPTR startDir = ((struct Process*)myTask)->pr_CurrentDir;

    /*
     * THE PORT IS MADE AFTER THE WAIT FOR CTRL_F, NOT BEFORE IT -- REVIEW-LIB.md P6.
     *
     * MasterControl ORs the PPC task's tc_SigAlloc into this process's own
     * (the ID_T68K arm, support68k.c) and sends CTRL_F only after that.
     * CreateMsgPort AllocSignal()s the port's bit on this process, and exec
     * hands out bit 31 first (portbit=1f on every line of the 22 Oct log,
     * FIXES.md item 19). Made BEFORE the OR, the port took bit 31 -- the bit
     * a PPC task gets from its first AllocSignalPPC(-1) as well, which counts
     * down from 31 too. The two sides then shared bit 31, andTemp (below) left
     * the port's bit out of everything that crosses, and a signal on it was
     * dropped for as long as the pair lived: the symptom of item 19, for a
     * task that reserved its bit before its first Run68K. Which of the two
     * ran first was the scheduler's; commit b0e3fc4 records the order that
     * hung on the user's hardware. Its "|=" in MasterControl closed that hang
     * and left the shared bit.
     *
     * 17.13b has the order by construction: its MirrorTask waits on its message
     * port for the creator's message before it calls CreateMsgPort
     * (powerpc.s:2120-2129), and MsgMir68 assigns the PPC task's signals to
     * TC_SIGALLOC before it sends that message (:2096-2098). Waiting for the
     * CTRL_F that MasterControl sends after its OR is the same order here, so
     * the bit allocated below is chosen against the PPC task's mask and cannot
     * be one it reserved.
     *
     * The flag clearing stays with the call it surrounds. A CreateMsgPort that
     * fails now returns after the Wait, so MasterControl's Signal finds this
     * process alive; before, it could return first and leave the Signal to hit
     * a process that had already gone.
     */
    ULONG mySignal = Wait(SIGBREAKF_CTRL_F);

    myTask->tc_Flags &= ~TF_PPC;

    if (!(mirrorPort = CreateMsgPort()))
    {
        PrintError(SysBase, "General memory allocation error");
        ((struct Process*)myTask)->pr_CurrentDir = startDir;
        return;
    }

    myTask->tc_Flags |= TF_PPC;

    if (myFrame = (struct MsgFrame*)myTask->tc_UserData)
    {
        PutMsg(mirrorPort, (struct Message*)myFrame);
    }

    /*
     * REGISTER THE PAIRING IN pp_MirrorList -- FIXES.md item 19.
     *
     * A grep of lib/ for pp_MirrorList finds one AddHead, in functions68k.c's
     * RunPPC at :218, which registers the 68k process that CALLED it. A
     * mirror spawned here by MasterControl for a self-started PPC task got
     * its pointers written on the PPC task (functionsppc.c:166-167 and
     * kernel.c:1265-1266) and no list entry, so the pairing was recorded in
     * one direction only.
     *
     * The 22 Oct sigstate log shows it directly: "SDL subtask" with
     * pt_Mirror68K=08e51058 and pt_MirrorPort=08c2eb18, its mirror running as
     * "SDL subtask_68K" -- and a pp_MirrorList holding one entry, for
     * ScummVM. Three places walk that list to resolve a task and none could
     * see this mirror: RunPPC at functions68k.c:181, which would build a
     * SECOND entry for the same task; the PPF_ASYNC check at :332; and the
     * cleanup at support68k.c:335, which sends ID_END to the PPC partner and
     * deletes the port when a mirror dies.
     *
     * HERE is the place, not MasterControl: the port is created by the
     * CreateMsgPort above, and the PPC task becomes known only when the first
     * frame arrives with the CTRL_F. Both are in hand at this point.
     *
     * This makes RunPPC from inside a mirror REUSE the entry rather than
     * duplicate it: functions68k.c:184 breaks out on a match with nxtMirror
     * non-NULL, which skips the creation block and uses the entry found. It
     * returns PPERR_ASYNCERR only for an entry marked PPF_ASYNC, and AllocVec
     * with MEMF_CLEAR leaves mt_Flags zero.
     */
    if (myMirror = (struct MirrorTask*)AllocVec(sizeof(struct MirrorTask),
                                                (MEMF_PUBLIC | MEMF_CLEAR)))
    {
        myMirror->mt_Task    = myTask;
        myMirror->mt_Port    = mirrorPort;
        myMirror->mt_PPCTask = myFrame ? myFrame->mf_PPCTask : NULL;

        Disable();

        AddHead((struct List*)&PowerPCBase->pp_MirrorList,
                (struct Node*)myMirror);

        Enable();
    }

    /* bits 0-11 do not cross; 0x010/0x100/0x400 are the PPC side's own
     * SIGF_SINGLE/SIGF_DOS/SIGF_WAIT. The reasoning, and the "+", are in
     * functions68k.c's copy of this expression */
    ULONG andTemp = ~(0xfff + (1 << (ULONG)mirrorPort->mp_SigBit));


    while (1)
    {
        /*  ONLY WAIT WHEN THE PORT IS EMPTY.
        **
        **  exec's Wait clears the bits it returns, so the port bit is gone
        **  before the first GetMsg below. That is safe on its own, because
        **  PutMsg's Signal sets the bit wherever this task happens to be --
        **  a message arriving during or after the drain re-sets it and the
        **  next Wait returns at once. It costs a spurious wake, never a
        **  lost message, PROVIDED nothing else on this task ever clears the
        **  bit. A sigdump snapshot of a ScummVM full screen stall shows the
        **  state that violates it: mirrorport queued=1, task in WAIT, port
        **  bit clear. Testing the list makes the loop independent of the
        **  signal, so a queued frame is serviced whatever happened to the
        **  bit. */
        mySignal = IsListEmpty((struct List*)&mirrorPort->mp_MsgList)
                     ? Wait((ULONG)myTask->tc_SigAlloc & 0xfffff000)
                     : (ULONG)(1 << (ULONG)mirrorPort->mp_SigBit);

        /* THE "else" BELOW IS THE ORIGINAL'S OWN SHAPE -- see the long note
         * in functions68k.c's identical loop. Forwarding unconditionally was
         * tried on 22 Oct and reverted: powerpc.s:4053-4075 takes the frame
         * branch the same way and keeps the other signals on the 68k task
         * for the next crossing, so the change moved WHEN a signal arrives,
         * not WHETHER, and it did not help on hardware. */
        if (mySignal & (1 << (ULONG)mirrorPort->mp_SigBit))
        {
            myTask->tc_SigRecvd |= (mySignal & andTemp);
            while (myFrame = (struct MsgFrame*)GetMsg(mirrorPort))
            {
            if (myFrame->mf_Identifier == ID_T68K)
            {
                /*
                 * WIDEN THE WAIT WITH THE PPC TASK'S ALLOCATION, the same
                 * change as functions68k.c's ID_T68K handler -- FIXES.md item
                 * 19, and the comment there carries the full reasoning,
                 * including why ASSIGNING this over tc_SigAlloc (as the
                 * original does at powerpc.s:3240) broke ScummVM outright
                 * here: it destroys the mirror's own mp_SigBit allocation,
                 * which the original never risks because it gives both sides
                 * the same bit number.
                 */
                /* assign, both ways, every crossing, keeping our own port
                 * bit -- the reasoning and the citations are in
                 * functions68k.c's copy of this handler */
                myTask->tc_SigAlloc = myFrame->mf_Arg[0]
                                    | (1 << (ULONG)mirrorPort->mp_SigBit);
                myTask->tc_SigRecvd |= myFrame->mf_Signals;
                myFrame->mf_MirrorPort = mirrorPort;

                Run68KCode(SysBase, &myFrame->mf_PPCArgs);

                /* not cleared: the copy below overwrites all 192 bytes (G1 D) */
                struct MsgFrame* doneFrame = CreateMsgFrameNoClear(PowerPCBase);

                CopyMemQuick((APTR)myFrame, (APTR)doneFrame, sizeof(struct MsgFrame));

                doneFrame->mf_Identifier = ID_DONE;

                /* atomic fetch-and-clear -- FIXES.md item 20, the same race
                 * as functions68k.c's ID_T68K handler, which carries the
                 * reasoning. A signal arriving between the capture and the
                 * clear was discarded by the xor. */
                doneFrame->mf_Signals    = SetSignal(0L, andTemp) & andTemp;
                doneFrame->mf_Arg[0]     = (ULONG)myTask;
                doneFrame->mf_Arg[1]     = myTask->tc_SigAlloc;

                SendMsgFrame(PowerPCBase, doneFrame);
                FreeMsgFrame(PowerPCBase, myFrame);
            }
            else if (myFrame->mf_Identifier == ID_END)
            {
                FreeMsgFrame(PowerPCBase, myFrame);

                /* take the entry registered above back out, so the list
                 * holds only live mirrors. The port is left alone: this
                 * function never deleted it, and changing that is a separate
                 * question from registering the pairing. */
                if (myMirror)
                {
                    Disable();
                    Remove((struct Node*)myMirror);
                    Enable();
                    FreeVec(myMirror);
                }
                ((struct Process*)myTask)->pr_CurrentDir = startDir;
                return;
            }
            else
            {
                FreeMsgFrame(PowerPCBase, myFrame);
                PrintError(SysBase, "68K mirror task received illegal command packet");
            }
            }
        }
        else if (mySignal & andTemp)
        {
            struct MsgFrame* crossFrame = CreateMsgFrame(PowerPCBase);
            crossFrame->mf_Identifier   = ID_LLPP;
            crossFrame->mf_Signals      = (mySignal & andTemp);
            crossFrame->mf_Arg[0]       = (ULONG)myTask;
            SendMsgFrame(PowerPCBase, crossFrame);
        }
    }
}

/********************************************************************************************/

FUNC68K void MasterControl(void)
{
	struct PrivatePPCBase* PowerPCBase = (struct PrivatePPCBase*)myPPCBase;
	struct ExecBase* SysBase = PowerPCBase->pp_PowerPCBase.PPC_SysLib;
    struct DosLibrary* DOSBase = PowerPCBase->pp_PowerPCBase.PPC_DosLib;
	struct MsgFrame* myFrame;
    struct MsgPort* mcPort;
	struct PPCZeroPage* myZP = (struct PPCZeroPage*)PowerPCBase->pp_PPCMemBase;

	if (!(mcPort = CreateMsgPort()))
    {
        PrintError(SysBase, "General memory allocation error");
        return;
    }

	PowerPCBase->pp_MasterControl = mcPort;

    ULONG mySignal = 0;

	while (!(mySignal & SIGBREAKF_CTRL_F))
	{
		mySignal = Wait(SIGBREAKF_CTRL_F);
	}

	struct Task* myTask = SysBase->ThisTask;

	//struct InternalConsts *myData = (struct InternalConsts*)myTask->tc_UserData;

    myZP->zp_Status = STATUS_INIT;

	while (1)
	{
		WaitPort(mcPort);

		while (myFrame = (struct MsgFrame*)GetMsg(mcPort))
		{
			myTask->tc_Flags |= TF_PPC;
			if (myFrame->mf_Message.mn_Node.ln_Type == NT_REPLYMSG)
			{
				if(myFrame->mf_Message.mn_Node.ln_Name)
				{
					struct MsgFrame* newFrame = CreateMsgFrame(PowerPCBase);
					newFrame->mf_Identifier = ID_XMSG;
					newFrame->mf_Arg[0] = (ULONG)myFrame;
					newFrame->mf_Message.mn_ReplyPort = (struct MsgPort*)myFrame->mf_Message.mn_Node.ln_Name;
					myFrame->mf_Message.mn_ReplyPort = (struct MsgPort*)myFrame->mf_Message.mn_Node.ln_Name;
					SendMsgFrame(PowerPCBase, newFrame);
				}
				else
				{
					PrintError(SysBase, "MasterControl received illegal reply message");
				}
			}
			else
			{
				switch (myFrame->mf_Identifier)
				{
					case ID_T68K:
					{
                        /*
                         * THE MIRROR'S NAME IS SIZED TO THE TASK'S, AND A START THAT FAILS IS
                         * TRIED AGAIN -- P5 (REVIEW-LIB.md).
                         *
                         * The name was built in a fixed 255-byte buffer through a UBYTE
                         * index. A PPC task name of 256 characters or more wrapped the index
                         * and never reached its end, so MasterControl looped for ever at
                         * priority 1 and starved every task at priority 0 or below; 251-255
                         * characters wrote past the buffer. WarpOS sizes it StrLen + 5
                         * (Create68KProc, powerpc.p:16008-16034), and 17.13b the same on its
                         * stack (MsgMir68, powerpc.s:2062-2075). The buffer is freed once the
                         * process exists: DOS copies NP_Name, which the stack buffer here and
                         * 17.13b's already relied on. It is taken with TF_PPC clear, in Amiga
                         * memory as that stack buffer was.
                         *
                         * A CreateNewProcTags that failed showed a requester and dropped the
                         * frame, so the PPC task waited in myWaitFor68K for ever. Now
                         * MasterControl waits a second and tries again. 17.13b retries at
                         * once (powerpc.s:2089), which spins at this priority for as long as
                         * memory is short; WarpOS does not check the process
                         * (powerpc.p:15966).
                         */
                        char *ppcName = myFrame->mf_PPCTask->tp_Task.tc_Node.ln_Name;
						char nameAppend[] = "_68K\0";
						ULONG nameLen = 0;
						char *Name68k;
						struct Process* myProc;

						while (ppcName[nameLen])
						{
							nameLen++;
						}

						myTask->tc_Flags &= ~TF_PPC;

						while (!(Name68k = AllocVec(nameLen + 5, MEMF_PUBLIC)))
						{
							Delay(50);
						}

						CopyMem(ppcName, Name68k, nameLen);
						CopyMem(&nameAppend, &Name68k[nameLen], 5);

						while (!(myProc = CreateNewProcTags(
						NP_Entry, (ULONG)&MirrorTask,
						NP_Name, (ULONG)Name68k,
						NP_Priority, 0,
						NP_StackSize, 0x20000,
						TAG_DONE)))
						{
							Delay(50);
						}

						FreeVec(Name68k);
						myTask->tc_Flags |= TF_PPC;

						myProc->pr_Task.tc_UserData = (APTR)myFrame;
						/*
						 * OR, NEVER ASSIGN -- FIXES.md item 19, and this was
						 * a RACE that hung a self-started PPC task.
						 *
						 * This was "=". MirrorTask called CreateMsgPort()
						 * BEFORE it waited for the CTRL_F below (it waits
						 * first since REVIEW-LIB.md P6, so this OR now
						 * always lands before its port is made), and
						 * CreateMsgPort AllocSignal()s a bit on that very
						 * Process. mf_Arg[0] is the PPC task's tc_SigAlloc,
						 * which for a CreateTaskPPC task is SYS_SIGALLOC =
						 * 0x0000FFFF (functionsppc.c:512). So when the
						 * assignment landed AFTER CreateMsgPort it wiped the
						 * port's own bit out of the mask -- and MirrorTask's
						 * loop waits on "tc_SigAlloc & 0xfffff000", which
						 * then evaluates to ZERO. Wait(0) blocks for ever,
						 * with the first frame already sitting on the port,
						 * so the PPC task's Run68K never returned.
						 *
						 * Landing BEFORE CreateMsgPort is harmless, because
						 * the bit is then allocated against the new mask and
						 * is present. That is the whole race, and it is why
						 * anything which perturbs the timing -- IDCMP traffic
						 * in windowed mode, or a disk write -- made the
						 * symptom disappear.
						 *
						 * ONLY THIS PATH IS AFFECTED, which matches where the
						 * user localised it: MasterControl's ID_T68K runs
						 * only for a PPC task that started itself and needs a
						 * 68k mirror made on demand. In the ordinary
						 * direction, a 68k program calling RunPPC, the mirror
						 * already exists and this code is never reached.
						 *
						 * The OR keeps both sides' allocations, which is what
						 * was intended: the mirror must know the bits the PPC
						 * task reserved, without forgetting its own. The same
						 * mistake, assigning over a task's own tc_SigAlloc,
						 * broke ScummVM outright when I made it in the
						 * ID_T68K handlers -- see the comment in
						 * functions68k.c.
						 *
						 * THIS ONE STAYS. It runs once, before the new mirror
						 * has made its port (P6: it waits for the CTRL_F
						 * sent below first): a seed, the 68k twin of the
						 * "tc_SigAlloc = mf_Arg[1]" that kernel.c gives a new
						 * PPC task. The per-crossing merges that made the two
						 * masks ratchet up to one union are the ones removed.
						 */
						myProc->pr_Task.tc_SigAlloc |= myFrame->mf_Arg[0];
						Signal((struct Task*)myProc, SIGBREAKF_CTRL_F);
						break;
					}
					case ID_LL68:
					{
                        ULONG (*DoLL68K_ptr)(__reg("d0") ULONG, __reg("d1") ULONG, __reg("a0") ULONG,
							 __reg("a1") ULONG, __reg("a6") ULONG) =
							 (APTR)(myFrame->mf_PPCArgs.PP_Regs[0] + myFrame->mf_PPCArgs.PP_Regs[1]);
						ULONG result = (*DoLL68K_ptr)(myFrame->mf_PPCArgs.PP_Regs[4], myFrame->mf_PPCArgs.PP_Regs[5],
								myFrame->mf_PPCArgs.PP_Regs[2], myFrame->mf_PPCArgs.PP_Regs[3],
								myFrame->mf_PPCArgs.PP_Regs[0]);
						struct MsgFrame* newFrame = CreateMsgFrame(PowerPCBase);
						newFrame->mf_PPCArgs.PP_Regs[0] = result;
						newFrame->mf_Identifier = ID_DNLL;
                        newFrame->mf_PPCTask = myFrame->mf_PPCTask;

                        SendMsgFrame(PowerPCBase, newFrame);
                        FreeMsgFrame(PowerPCBase, myFrame);

						break;
					}
#if 0
					case ID_____:
					{
						
                        void (*FreeMem_ptr)(__reg("d0") ULONG, __reg("a1") ULONG, __reg("a6") struct ExecBase*) =
							 (APTR)(myFrame->mf_PPCArgs.PP_Regs[0] + myFrame->mf_PPCArgs.PP_Regs[1]);
						(*FreeMem_ptr)(myFrame->mf_PPCArgs.PP_Regs[4], myFrame->mf_PPCArgs.PP_Regs[3], SysBase);
						FreeMsgFrame(PowerPCBase, myFrame);
						break;
					}
#endif
					case ID_DBGS:
					{
                        struct DebugArgs* dbargs = (struct DebugArgs*)&myFrame->mf_PPCArgs;
                        ULONG numargs = dbargs->db_Function >> 8;
                        dbargs->db_Function = FuncStrings[(dbargs->db_Function & 0xff)];
                        switch (numargs)
                        {
                            case 1:
                            {
                                mySPrintF68K((struct PPCBase*)PowerPCBase, "%s -> %s(%08lx)\n\0", (APTR)dbargs);
                                break;
                            }
                            case 2:
                            {
                                mySPrintF68K((struct PPCBase*)PowerPCBase, "%s -> %s(%08lx, %08lx)\n\0", (APTR)dbargs);
                                break;
                            }
                            case 3:
                            {
                                mySPrintF68K((struct PPCBase*)PowerPCBase, "%s -> %s(%08lx, %08lx, %08lx)\n\0", (APTR)dbargs);
                                break;
                            }
                            case 4:
                            {
                                mySPrintF68K((struct PPCBase*)PowerPCBase, "%s -> %s(%08lx, %08lx, %08lx, %08lx)\n\0", (APTR)dbargs);
						        break;
                            }
                            default:
                            {
                                mySPrintF68K((struct PPCBase*)PowerPCBase, "%s -> %s()\n\0", (APTR)dbargs);
                               break;
                            }

                        }
                        FreeMsgFrame(PowerPCBase, myFrame);
						break;
					}
					case ID_DBGE:
					{
                        struct DebugArgs* dbargs = (struct DebugArgs*)&myFrame->mf_PPCArgs;
                        dbargs->db_Function = FuncStrings[(dbargs->db_Function & 0xff)];
                        mySPrintF68K((struct PPCBase*)PowerPCBase, "%s -> %s() = %08lx\n\0", (APTR)dbargs);
						FreeMsgFrame(PowerPCBase, myFrame);
						break;
					}
					case ID_CRSH:
					{
                        struct DosLibrary* DOSBase = PowerPCBase->pp_PowerPCBase.PPC_DosLib;
						FreeMsgFrame(PowerPCBase, myFrame);
						BPTR excWindow;
						if (!(excWindow = Open("CON:0/20/680/250/PowerPC Exception/AUTO/CLOSE/WAIT/INACTIVE", MODE_NEWFILE)))
						{
							PrintError(SysBase, "PPC crashed but could not output crash window");
							break;
						}
						if(!(*((ULONG*)(PowerPCBase->pp_PPCMemBase + (ULONG)(FIFO_END + 0x100)))))
						{
							*((ULONG*)(PowerPCBase->pp_PPCMemBase + (ULONG)(FIFO_END + 0x100))) = (ULONG)&msgPanic;
						}
                        ULONG* errorData = (ULONG*)(PowerPCBase->pp_PPCMemBase + (ULONG)(FIFO_END + 0x100));
                        errorData[2] = ExcStrings[(errorData[2])];

						VFPrintf(excWindow, (STRPTR)&CrashMessage, (LONG*)(PowerPCBase->pp_PPCMemBase + (ULONG)(FIFO_END + 0x100)));
#ifdef DEBUG
                        RawDoFmt((STRPTR)&CrashMessage, (APTR)errorData, &DPutChProc, (APTR)SysBase);
#endif
                        switch (*((ULONG*)(PowerPCBase->pp_PPCMemBase + (ULONG)(FIFO_END + 0x14c))))
						{
							case ERR_ESEM:
							{
								PrintError(SysBase, "PPC Semaphore in illegal state");
                                D(("PPC Semaphore in illegal state\n"));
								break;
							}
							case ERR_EFIF:
							{
								PrintError(SysBase, "PPC received an illegal command packet");
                                D(("PPC received an illegal command packet\n"));
								break;
							}
							case ERR_ESNC:
							{
								PrintError(SysBase, "Async Run68K function not supported");
                                D(("Async Run68K function not supported\n"));
								break;
							}
							case ERR_EMEM:
							{
								PrintError(SysBase, "PPC CPU ran out of memory");
                                D(("PPC CPU ran out of memory\n"));
								break;
							}
							case ERR_ECOR:
							{
								PrintError(SysBase, "PPC Memory corruption detected during freeing");
                                D(("PPC Memory corruption detected during freeing\n"));
								break;
							}
							case ERR_ETIM:
							{
								PrintError(SysBase, "PPC timed out while waiting on 68K");
                                D(("PPC timed out while waiting on 68K\n"));
								break;
							}
							case ERR_R68K:
							{
								PrintError(SysBase, "Illegal Run68K function execution detected");
                                D(("Illegal Run68K function execution detected\n"));
								break;
							}
						}
						break;
					}
					default:
					{
                        PrintError(SysBase, "68K received an illegal command packet");
                        D(("68K received an illegal command packet\n"));
						break;
					}
				}
			}
		}
	}
}

/********************************************************************************************
*
*	Interrupt that gets called by the PPC card.
*
*********************************************************************************************/

FUNC68K ULONG GortInt(__reg("a1") APTR data, __reg("a5") APTR code)
{
    struct PrivatePPCBase* PowerPCBase = (struct PrivatePPCBase*)myPPCBase;
    struct ExecBase* SysBase = PowerPCBase->pp_PowerPCBase.PPC_SysLib;
	struct MsgFrame* myFrame;
	ULONG flag = 0;

    /*
     * Q10: ONE GortInt AT A TIME (gortBusy, above). Set before the first
     * frame is taken and cleared after the last, at the one exit below.
     */
    if (gortBusy)
    {
        return 0;
    }
    gortBusy = 1;

    switch (PowerPCBase->pp_DeviceID)
	{
		case DEVICE_HARRIER:
		{
			if (readmemL(PowerPCBase->pp_BridgeMsgs, PMEP_MIST))
            {
                flag = 1;
            }
            break;
		}
		case DEVICE_MPC107:
		{
			if ((readmemL(PowerPCBase->pp_BridgeMsgs, MPC107_OMISR)) & MPC107_OPQI)
            {
                flag = 1;
            }
            break;
		}
		case DEVICE_MPC8343E:
		{
            if ((data == (APTR)SUPERKEY) || (readmemL(PowerPCBase->pp_BridgeConfig, IMMR_OMISR) & IMMR_OMISR_OM0I))
			{
                writememL(PowerPCBase->pp_BridgeConfig, IMMR_OMISR, IMMR_OMISR_OM0I);
                flag = 1;
			}
            break;
		}
        case DEVICE_MPC8314E:
        {
            if ((data == (APTR)SUPERKEY) || (readmemL(PowerPCBase->pp_BridgeConfig, IMMR_PEX_OMBCR)))
			{
                writememL(PowerPCBase->pp_BridgeConfig, IMMR_PEX_OMBCR, 0);
                writememL(PowerPCBase->pp_BridgeConfig, IMMR_PEX_HISR, PEX_HISR_OMB);
                flag = 1;
			}
            break;
        }
		default:
		{
			break; //error
		}
	}
    if (flag)
    {
        while ((myFrame = GetMsgFrame(PowerPCBase)) != (APTR)-1)
		{
            switch (myFrame->mf_Identifier)
			{
				case ID_T68K:
				case ID_END:
				{
                    struct MsgPort* mirrorPort;
                    if (mirrorPort = myFrame->mf_MirrorPort)
					{
						if (myFrame->mf_Identifier != ID_END)
                        {
                            /* assign, keeping the mirror's port bit -- as
                             * Sonnet's MsgT68k does from the same place,
                             * powerpc.s:2536; see functions68k.c */
                            struct Task* sigTask = mirrorPort->mp_SigTask;
						    sigTask->tc_SigAlloc = myFrame->mf_Arg[0]
                                                 | (1 << (ULONG)mirrorPort->mp_SigBit);
						}
                        PutMsg(mirrorPort, &myFrame->mf_Message);
                    }
					else
					{
                        PutMsg(PowerPCBase->pp_MasterControl, &myFrame->mf_Message);
					}
					break;
				}
				case ID_FPPC:
				{
                    struct MsgPort* replyPort = myFrame->mf_Message.mn_ReplyPort;
					/* assign, keeping the caller's port bit -- Sonnet's
					 * MsgFPPC, powerpc.s:2546 */
					struct Task* sigTask = replyPort->mp_SigTask;
					sigTask->tc_SigAlloc = myFrame->mf_Arg[0]
					                     | (1 << (ULONG)replyPort->mp_SigBit);
					ReplyMsg(&myFrame->mf_Message);
					break;
				}
				case ID_XMSG:
				{
					struct MsgPort* xPort = (struct MsgPort*)myFrame->mf_Arg[0];
					struct Message* xMessage = (struct Message*)myFrame->mf_Arg[1];
					xMessage->mn_Node.ln_Name = (APTR)xMessage->mn_ReplyPort;
					xMessage->mn_ReplyPort = PowerPCBase->pp_MasterControl;
					FreeMsgFrame(PowerPCBase, myFrame);
					/*
					 * F6: THE RECEIVER SEES THE XMsg TYPE, NT_XMSGPPC, as on
					 * WarpOS, whose PutXMsg68K links the message without
					 * touching it (powerpc.s:6207-6212). exec's PutMsg sets
					 * NT_MESSAGE, so the type is put back before anyone sees
					 * the message: GortInt runs as an interrupt (libinit.c's
					 * server, and ZenInt's below), so no task or soft
					 * interrupt runs before it returns. myFreeXMsg
					 * (functions68k.c) reads it.
					 */
					PutMsg(xPort, xMessage);
					xMessage->mn_Node.ln_Type = NT_XMSGPPC;
					break;
				}
				case ID_SIG:
				{
					Signal((struct Task*)myFrame->mf_Arg[0], myFrame->mf_Signals);
					FreeMsgFrame(PowerPCBase, myFrame);
					break;
				}
				case ID_RX68:
				{
					struct Message* xMessage = (struct Message*)myFrame->mf_Arg[0];
					struct MsgPort* xPort = xMessage->mn_ReplyPort;
					FreeMsgFrame(PowerPCBase, myFrame);
					PutMsg(xPort, xMessage);
					break;
				}
				case ID_GETV:
				{
                    myFrame->mf_Arg[0] = *((ULONG*)(myFrame->mf_Arg[1]));
#ifdef DEBUG
                    if (!(countDSI(myFrame->mf_Arg[1])))
                    {
                        myFrame->mf_Arg[0] = ERR_EMEM;
                    }
#endif
					myFrame->mf_Identifier = ID_DONE;
					FreeMsgFrame(PowerPCBase, myFrame);
					break;
				}
				case ID_PUTW:
				{
                    *((ULONG*)(myFrame->mf_Arg[1])) = myFrame->mf_Arg[0];
					myFrame->mf_Identifier = ID_DONE;
#ifdef DEBUG
                    countDSI(myFrame->mf_Arg[1]);
#endif

					FreeMsgFrame(PowerPCBase, myFrame);
					break;
				}
				case ID_PUTH:
				{
                    *((USHORT*)(myFrame->mf_Arg[1])) = (USHORT)myFrame->mf_Arg[0];
					myFrame->mf_Identifier = ID_DONE;
#ifdef DEBUG
                    countDSI(myFrame->mf_Arg[1]);
#endif
					FreeMsgFrame(PowerPCBase, myFrame);
					break;
				}
				case ID_PUTB:
				{
                    *((UBYTE*)(myFrame->mf_Arg[1])) = (UBYTE)myFrame->mf_Arg[0];
					myFrame->mf_Identifier = ID_DONE;
#ifdef DEBUG
                    countDSI(myFrame->mf_Arg[1]);
#endif
					FreeMsgFrame(PowerPCBase, myFrame);
					break;
				}
				default:
				{
					PutMsg(PowerPCBase->pp_MasterControl, (struct Message*)myFrame);
                    break;
				}
			}
		}
    }
    /*
     * Q12: ALWAYS "NOT MINE", SO EXEC GOES ON DOWN THE INT2 CHAIN. GortInt
     * shares INT2 with the other cards' servers (the Radeon's vertical blank,
     * SCSI). A non-zero return ends exec's walk of the chain, so a server
     * behind GortInt was not called for an interrupt of its own that came
     * with the card's. Each server tests its own source, so going on costs a
     * few instructions. 17.13b's SonInt answers "handled" (powerpc.s:2351-2354);
     * the Radeon driver answers 0 for the same reason (its Interrupt.c:105-122).
     * flag still decides above whether the queue is drained. tests/int2scan.
     */
    gortBusy = 0;
    return 0;
}

/********************************************************************************************
*
*	VBLANK Interrupt that supports unreliable (for now) messaging from K1/M1
*
*********************************************************************************************/

FUNC68K ULONG ZenInt(__reg("a1") APTR data, __reg("a5") APTR code)
{

    struct PrivatePPCBase* PowerPCBase = (struct PrivatePPCBase*)myPPCBase;
    struct Custom* custom = (struct Custom*)CUSTOMBASE;

	ULONG configBase;
	ULONG pending;

    /* Q10: this blank interrupted GortInt, which drains the queue itself */
    if (gortBusy)
    {
        return 0;
    }

    /*
     * Q11 A: IS THE CARD'S MESSAGE INTERRUPT PENDING? The K1 and the PCI M1
     * post through OMR0, which sets OMISR[OM0I]. The PCIe M1 posts by setting
     * PEX_OMBCR[READY], which sets PEX_HISR[OMB] (MPC8315E RM, PDF p. 758)
     * and leaves OMISR clear, so the OMISR test found nothing pending at every
     * blank there and this server took any waiting message. A pending
     * interrupt is GortInt's; a waiting message is picked up here only when
     * none is pending, on all three cards. 17.15 had a PCIe test commented
     * out here, on PEX_OMBCR, whose READY stays set after a lost interrupt.
     * PEX_HISR_OMB is the 68k's view, as GortInt writes it.
     */
	configBase = PowerPCBase->pp_BridgeConfig;
	if (PowerPCBase->pp_DeviceID == DEVICE_MPC8314E)
	{
		pending = readmemL(configBase, IMMR_PEX_HISR) & PEX_HISR_OMB;
	}
	else
	{
		pending = readmemL(configBase, IMMR_OMISR) & IMMR_OMISR_OM0I;
	}

	if (!pending)
	{
		struct killFIFO* myFIFO = (struct killFIFO*)((ULONG)(PowerPCBase->pp_PPCMemBase + FIFO_END));
		if (myFIFO->kf_MIOPT != myFIFO->kf_MIOPH)
		{
            custom->intena = INTF_PORTS;
            data = (APTR)SUPERKEY;
			GortInt(data, code);
			custom->intena = INTF_SETCLR|INTF_INTEN|INTF_PORTS;
		}
	}
	return 0;
}

/********************************************************************************************
*
*	Functions that creates a message for the PPC
*
*********************************************************************************************/

/*
 * G1 D: THE FRAME COMES BACK AS IT WAS LEFT, NOT CLEARED. CreateMsgFrame below clears all 192
 * bytes with 48 writes over PCI, about 33 us a call on both cards (a 68k write to PPC RAM costs
 * 0.69 us: tests/rtbreak, 12 Nov 2026). A reply frame is filled by a copy of the request that
 * overwrites every byte, so the two reply sites, the ID_T68K handlers of functions68k.c and of
 * MirrorTask, take this one. 17.13b clears no frame it creates (powerpc.s:2780-2818, read 12
 * Nov): the clearing is this library's own and the other callers keep it.
 */
struct MsgFrame* CreateMsgFrameNoClear(struct PrivatePPCBase* PowerPCBase)
{
    struct ExecBase* SysBase = PowerPCBase->pp_PowerPCBase.PPC_SysLib;
    struct killFIFO* myFIFO = (struct killFIFO*)((ULONG)(PowerPCBase->pp_PPCMemBase + FIFO_END));
    ULONG msgFrame = NULL;

    /*
     * NEVER AN EMPTY READ (Q4, REVIEW-LIB.md). An empty inbound free list
     * reads 0xFFFF_FFFF from the Harrier's MIIQ (its manual, 3-105) and, by
     * the I2O convention, from the MPC107's IFQPR; Killer's software ring is
     * empty when kf_MIIFH == kf_MIIFT. That read was returned as a frame, and
     * the clear below wrote zeros from 0xFFFFFFFF through 0xBB -- AbsExecBase
     * and the vectors; a second one spun here for ever, as it equalled
     * kf_CreatePrevious. Now an empty read is retried, with interrupts let in
     * between, and never becomes kf_CreatePrevious. Every caller is a task.
     * 17.13b has no empty test either (powerpc.s:2780-2818): THIS DIVERGES,
     * rightly. WarpOS has no fixed ring (AllocJob, powerpc.s:4395).
     */
    Disable();

    while (1)
    {
        switch (PowerPCBase->pp_DeviceID)
        {
            case DEVICE_HARRIER:
            {
                msgFrame = readmemL(PowerPCBase->pp_BridgeMsgs, PMEP_MIIQ);
                break;
            }
            case DEVICE_MPC8343E:
            case DEVICE_MPC8314E:
            {
                /*
                 * G1 D: EACH RING POINTER IS READ ONCE (see GetMsgFrame): kf_MIIFT, the 68k's own,
                 * was read three times a call. 17.13b reads it once (powerpc.s:2796-2804).
                 */
                ULONG tail = myFIFO->kf_MIIFT;

                if (*((volatile ULONG*)&myFIFO->kf_MIIFH) == tail)
                {
                    msgFrame = 0xFFFFFFFF;              //Q4: empty; the PPC writes kf_MIIFH
                }
                else
                {
                    msgFrame = *((ULONG*)tail);
                    myFIFO->kf_MIIFT = (tail + 4) & 0xffff3fff;
                }
                break;
            }
            case DEVICE_MPC107:
            {
                msgFrame = readmemL(PowerPCBase->pp_BridgeMsgs, MPC107_IFQPR);
                break;
            }
        }
        if (msgFrame == 0xFFFFFFFF)
        {
            Enable();               //Q4: empty; interrupts in while the PPC frees one
            Disable();
            continue;
        }
        if (msgFrame != myFIFO->kf_CreatePrevious)
        {
            myFIFO->kf_CreatePrevious = msgFrame;
            break;
        }
    }
    Enable();

    return (struct MsgFrame*)msgFrame;
}

/*
 * The same frame with every one of its 48 longwords cleared, for a caller that fills in a few
 * fields and sends it, so that the receiver finds the rest zero, as it always did: RunPPC's, the
 * cross frames, MasterControl's (each caller read 12 Nov).
 */
struct MsgFrame* CreateMsgFrame(struct PrivatePPCBase* PowerPCBase)
{
    ULONG msgFrame = (ULONG)CreateMsgFrameNoClear(PowerPCBase);

    if (msgFrame)
    {
        ULONG clearFrame = msgFrame;
        for (int i=0; i<48; i++)
        {
            *((ULONG*)(clearFrame)) = 0;
            clearFrame += 4;
        }
    }
    return (struct MsgFrame*)msgFrame;
}

/********************************************************************************************
*
*	Function that sends a message to the PPC and subsequently interrupts the PPC
*
*********************************************************************************************/

void SendMsgFrame(struct PrivatePPCBase* PowerPCBase, struct MsgFrame* msgFrame)
{
    struct ExecBase* SysBase = PowerPCBase->pp_PowerPCBase.PPC_SysLib;

    Disable();

    switch (PowerPCBase->pp_DeviceID)
    {
        case DEVICE_HARRIER:
        {
            writememL(PowerPCBase->pp_BridgeMsgs, PMEP_MIIQ, (ULONG)msgFrame);
            break;
        }

        case DEVICE_MPC8343E:
        case DEVICE_MPC8314E:
        {
            struct killFIFO* myFIFO = (struct killFIFO*)((ULONG)(PowerPCBase->pp_PPCMemBase + FIFO_END));

            ULONG head = myFIFO->kf_MIIPH;          //G1 D: once, the 68k's own pointer (17.13b: powerpc.s:2842-2849)

            *((ULONG*)head) = (ULONG)msgFrame;
            myFIFO->kf_MIIPH = (head + 4) & 0xffff3fff;
            if (PowerPCBase->pp_DeviceID == DEVICE_MPC8343E)
            {
                writememL(PowerPCBase->pp_BridgeConfig, IMMR_IMR0, (ULONG)msgFrame);
            }
            else
            {
                writememL(PowerPCBase->pp_BridgeConfig, IMMR_PEX_IMBDR, (ULONG)msgFrame);
                writememL(PowerPCBase->pp_BridgeConfig, IMMR_PEX_IMBCR, swap32(PEX_IMBCR_READY));
            }
            break;
        }

        case DEVICE_MPC107:
        {
            writememL(PowerPCBase->pp_BridgeMsgs, MPC107_IFQPR, (ULONG)msgFrame);
            break;
        }

        default:
        {
            break;
        }
    }

    Enable();

    return;
}

/********************************************************************************************
*
*	Function that frees a message send to the 68K processor by the PPC
*
*********************************************************************************************/

void FreeMsgFrame(struct PrivatePPCBase* PowerPCBase, struct MsgFrame* msgFrame)
{
    struct ExecBase* SysBase = PowerPCBase->pp_PowerPCBase.PPC_SysLib;

    Disable();

    msgFrame->mf_Identifier = ID_FREE;

    switch (PowerPCBase->pp_DeviceID)
    {
        case DEVICE_HARRIER:
        {
            ULONG msgOffset = readmemL(PowerPCBase->pp_BridgeConfig, XCSR_MIOFH);
            /*
             * THE SLOT BEFORE THE HEAD (Q8, REVIEW-LIB.md): a PPC waiting for
             * MIOFH to move (Q4) pops the slot at once. 17.13b writes the slot
             * (powerpc.s:2891), then MIOFH (:2892); this had them reversed.
             */
            writememL(PowerPCBase->pp_PPCMemBase, msgOffset, (ULONG)msgFrame);
            writememL(PowerPCBase->pp_BridgeConfig, XCSR_MIOFH, (msgOffset + 4) & 0xffff3fff);
            break;
        }
        case DEVICE_MPC8343E:
        case DEVICE_MPC8314E:
        {
            struct killFIFO* myFIFO = (struct killFIFO*)((ULONG)(PowerPCBase->pp_PPCMemBase + FIFO_END));

            ULONG head = myFIFO->kf_MIOFH;          //G1 D: once, the 68k's own pointer (17.13b: powerpc.s:2897-2903)

            *((ULONG*)head) = (ULONG)msgFrame;
            myFIFO->kf_MIOFH = (head + 4) & 0xffff3fff;
            break;
        }
        case DEVICE_MPC107:
        {
            writememL(PowerPCBase->pp_BridgeMsgs, MPC107_OFQPR, (ULONG)msgFrame);
            break;
        }
    }

    Enable();

    return;
}

/********************************************************************************************
*
*	Function that receives a message from the FIFO which was send to the 68K by the PPC
*
*********************************************************************************************/

struct MsgFrame* GetMsgFrame(struct PrivatePPCBase* PowerPCBase)
{
    struct ExecBase* SysBase = PowerPCBase->pp_PowerPCBase.PPC_SysLib;
    struct killFIFO* myFIFO = (struct killFIFO*)((ULONG)(PowerPCBase->pp_PPCMemBase + FIFO_END));
    ULONG msgFrame;

    Disable();

    while (1)
    {
        /*
         * EVERY PASS STARTS EMPTY (M1, REVIEW-LIB.md). The Harrier and Killer
         * arms leave msgFrame alone on an empty queue, and it was set to -1
         * once, before the loop: a pass that read a duplicate of
         * kf_GetPrevious and then found the queue empty kept the duplicate
         * and went round for ever, under Disable. 17.13b sets -1 on every
         * empty pass (powerpc.s:2937-2939, :2948-2950): THIS MATCHES. NO
         * COUNTERPART in WarpOS, which has no frame queues.
         */
        msgFrame = -1;

        switch (PowerPCBase->pp_DeviceID)
        {
            case DEVICE_HARRIER:
            {
                if (readmemL(PowerPCBase->pp_BridgeConfig, XCSR_MIOPT) ==
                    readmemL(PowerPCBase->pp_BridgeConfig, XCSR_MIOPH))
                {
                    break;
                }
                else
                {
                    ULONG msgOffset = readmemL(PowerPCBase->pp_BridgeConfig, XCSR_MIOPT);
                    msgFrame = readmemL(PowerPCBase->pp_PPCMemBase, msgOffset);
                    writememL(PowerPCBase->pp_BridgeConfig, XCSR_MIOPT, (msgOffset + 4) & 0xffff3fff);
                }
                break;
            }
            case DEVICE_MPC8343E:
            case DEVICE_MPC8314E:
            {
                /*
                 * G1 D: EACH RING POINTER IS READ ONCE. A field of this structure is a read over
                 * PCI, 0.98 us on the PCI M1 and 1.87 us on the PCIe M1 behind its adapter
                 * (tests/rtbreak, 12 Nov 2026), and the compiler does not merge them: kf_MIOPT
                 * was read three times a call. 17.13b reads it once (powerpc.s:2943-2957): this
                 * MATCHES it now. kf_MIOPT is the 68k's own, so one read is exact; kf_MIOPH is
                 * the PPC's and is read once, through a volatile access.
                 */
                ULONG tail = myFIFO->kf_MIOPT;

                if (tail == *((volatile ULONG*)&myFIFO->kf_MIOPH))
                {
                    break;
                }
                else
                {
                    msgFrame = *((ULONG*)tail);
                    myFIFO->kf_MIOPT = (tail + 4) & 0xffff3fff;
                }
                break;
            }
            case DEVICE_MPC107:
            {
                msgFrame = readmemL(PowerPCBase->pp_BridgeMsgs, MPC107_OFQPR);
                break;
            }
        }
        if (msgFrame == -1)
        {
            break;
        }
        else if (msgFrame != myFIFO->kf_GetPrevious)
        {
            myFIFO->kf_GetPrevious = msgFrame;
            break;
        }
    }

    Enable();

    return (struct MsgFrame*)msgFrame;
}


//CreateProc, CreateNewProc, RunCommand, SystemTagList
