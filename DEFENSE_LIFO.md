# Correction-sheet recode: FIFO to LIFO

This is a **temporary defense exercise**, not a new mandatory scheduler.
The normal submitted program must continue to implement FIFO and EDF.
The sheet asks you to change the behavior selected by the existing `fifo`
argument, then compare the same workload before and after the change.

## Where to make the change

Open `src/heap.c`, function `heap_before()`. The request sequence increases
whenever a request is queued. A smaller sequence means an older request;
a larger sequence means a newer request.

The normal comparison is:

```c
if (x->request_seq != y->request_seq)
    return (x->request_seq < y->request_seq);
```

For the temporary exercise, replace that block with:

```c
if (x->request_seq != y->request_seq)
{
    if (sim->cfg.policy == FIFO)
        return (x->request_seq > y->request_seq);
    return (x->request_seq < y->request_seq);
}
```

Preserve the preceding EDF deadline/slot checks and the final ID tie-break.
Indent using tabs in the actual C file to respect the Norm.

With this change, the FIFO branch becomes LIFO. `heap_push()` and `heap_pop()`
already use the comparator, so no separate stack implementation is required.
Keep the command-line argument `fifo` for this exercise; do not add `lifo` to
normal argument parsing. The grant code still requires the root of both heaps,
so the newest waiting request wins each contested dongle.

## How to demonstrate it

Before the temporary change:

```sh
make
./codexion 5 3000 200 200 200 10 800 fifo > /tmp/codexion-before-lifo.log
```

After the temporary change:

```sh
make
./codexion 5 3000 200 200 200 10 800 fifo > /tmp/codexion-after-lifo.log
norminette src
```

Compare the compilation sequences, but do not claim that a globally reversed
sequence proves LIFO. New requests arrive during execution, and stdout does
not explicitly show queue insertion times. Explain the comparator and inspect
individual queued-request decisions with the evaluator. A controlled heap test
with sequences 1 and 2 should yield sequence 2 first for the temporary branch,
while the normal FIFO branch yields sequence 1 first.

Burnout and starvation can happen in this exercise; the sheet explicitly allows
them. Dongle ownership must remain exclusive and logs must stay serialized.
Revert only this temporary comparator edit and rebuild before submitting.
Do not reset other project changes.

## What remains your responsibility

The evaluator is checking whether **you** can explain and perform the recode.
A prepared example does not satisfy that live part of the evaluation by itself.
