# ppctasks (17.18)

A 68k tool that lists every PPC task the library knows, read from `PrivatePPCBase` (`pp_AllTasks`) under
`Disable()`: where it is (`R` running, `r` ready, `w` waiting, `n` new), its state, priority, wait and
received signals, signal allocation, the scheduler's fields (quantum, elapsed, total, priority, offset,
desired, NICE, activity, busy) and the `WaitTime()` list with each deadline against `pp_CurrentTBL`.

It is built against this directory's headers, so it must run with a library built from the same sources:

```sh
vc +aos68k -c99 -O1 -cpp-comments -I../.. -I../../cross/include -I../../cross/third_party/warpos \
   -I/opt/amiga_sdk/NDK3.2R4/Include_H ppctasks.c -o ppctasks -lamiga
```

For a hang: which PPC task waits for which signal, whether anything is ready, and (run it twice) which task's
total CPU time grows. A `WaitTime()` deadline that moves between two runs is a task polling, not a stuck timer.

It found the 17.16/17.17 Settlers II hang (2026-11-16): the intro player AMPS2 polling with `WaitTime()`, its
plugins and audio tasks waiting on signals nobody sent, and every task at priority 0 with offset 0, so the
V14 rule (switch only to a strictly higher priority) never pre-empted anything. 17.18 lets an equal-priority
ready task take the CPU again.
