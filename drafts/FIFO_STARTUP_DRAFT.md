# Draft: coordinate the first FIFO requests on an even ring

**Status: applied to the main project at the user's request.** The original
[fifo_startup.patch](fifo_startup.patch) is retained as a record of the change.
Do not apply it again to the current source. The fix targets the reproduced
`10 325 100 100 100 10 0 fifo` failure.

## What goes wrong in the current code

In `threads.c`, every worker is created before the common start is released:

```c
sim->ready = 1;
pthread_cond_broadcast(&sim->changed);
```

In `worker.c`, workers wait for that release:

```c
while (!sim->ready)
    pthread_cond_wait(&sim->changed, &sim->mutex);
```

This is already a useful startup barrier. The issue is what happens afterward:
which worker runs and obtains `sim->mutex` first is not controlled by the
creation loop or broadcast. The program has no FIFO startup admission order.

In `acquire.c`, each worker creates its request:

```c
coder->request_seq = ++sim->seq;
heap_push(sim, &sim->dongles[coder->left].queue, coder->id - 1);
heap_push(sim, &sim->dongles[coder->right].queue, coder->id - 1);
```

In `grant.c`, it must lead both queues:

```c
heap_top(&a->queue) == coder->id - 1
    && heap_top(&b->queue) == coder->id - 1
```

That respects FIFO, but the initial order can create a chain of dependencies.
A coder can lead one queue while waiting behind another coder on its second
queue. The first queue then delays its other neighbor even when that dongle is
free. There is no partial dongle ownership or mutex deadlock here; it is queue
blocking that can last long enough to miss a deadline.

In one reproduced failure, coder 2 starts at 0 ms, debugs at 100 ms, refactors
at 201 ms, and becomes ready to request again around 301 ms. Coder 3's older
first request starts compiling at 302 ms and holds their shared dongle until
approximately 402 ms. Coder 2 burns out at its 325-ms deadline.

Another failure burns out a coder before its first compilation. This is not
explained by accumulating small sleep inaccuracies over ten cycles.

## Proposed design

For **FIFO with an even number of coders**, coordinate only the first request:

1. All workers use the existing start barrier.
2. Admit first requests in odd-ID order, then even-ID order.
3. Wait until every first request has actually been inserted into both heaps.
4. Allow normal resource acquisition to proceed.
5. Submit all later requests immediately after refactoring, using ordinary FIFO.

For ten coders, the initial request order is:

```text
1, 3, 5, 7, 9, 2, 4, 6, 8, 10
```

Each odd coder needs a pair disjoint from every other odd coder. Every odd
request therefore leads both of its heaps. The even requests follow them.
There is no need to reverse the comparator or let a later request bypass an
older one: control admission before a request enters the queues.

The second barrier is essential. Without it, a worker might start compiling
and issue another request before a delayed worker registers its first one.

This controls logical admission order, not CPU scheduling. The OS still decides
when a runnable thread executes. No hard real-time guarantee is implied.

## Concrete implementation

Add one zero-initialized field to `t_sim`:

```c
int fifo_queued;
```

`init_sim()` already zeroes the entire structure. The field is read and written
only while `sim->mutex` is held. The new helper is called with that mutex held:

```c
void queue_first_fifo_locked(t_coder *coder)
{
    t_sim *sim;
    int rank;

    sim = coder->sim;
    rank = (coder->id - 1) / 2;
    if (coder->id % 2 == 0)
        rank += sim->cfg.count / 2;
    while (!sim->stop && sim->fifo_queued != rank)
        pthread_cond_wait(&sim->changed, &sim->mutex);
    if (sim->stop)
        return;
    queue_request_locked(coder);
    sim->fifo_queued++;
    pthread_cond_broadcast(&sim->changed);
    while (!sim->stop && sim->fifo_queued < sim->cfg.count)
        pthread_cond_wait(&sim->changed, &sim->mutex);
}
```

`rank` is a zero-based insertion position. For ten coders, IDs 1/3/5/7/9 have
ranks 0/1/2/3/4 and IDs 2/4/6/8/10 have ranks 5/6/7/8/9.

The first loop waits for that worker's insertion turn. The final loop prevents
any worker from returning to acquisition until all ten requests are queued.
Both waits release the mutex while sleeping, allowing the next worker or monitor
to run. A stop broadcast interrupts either loop. The queued heaps are freed
normally during shutdown.

At the beginning of `await_pair_locked()` in `acquire.c`:

```c
if (sim->cfg.policy == FIFO && sim->cfg.count % 2 == 0
    && coder->compiles == 0)
    queue_first_fifo_locked(coder);
else if (sim->cfg.policy != EDF || coder->compiles > 0)
    queue_request_locked(coder);
```

After that, the existing grant/wait loop is unchanged. The patch also declares
the helper in the header and adds `src/fifo_start.c` to the Makefile.

## Expected schedule for the reported case

Ignoring execution overhead, resource use can look like this:

| Group | First compile | Debug | Refactor | Next compile |
| --- | --- | --- | --- | --- |
| Odd IDs | 0–100 ms | 100–200 ms | 200–300 ms | 300–400 ms |
| Even IDs | 100–200 ms | 200–300 ms | 300–400 ms | 400–500 ms |

Those are expected resource phases, not hardcoded timestamps. Each group
compiles concurrently within itself; each coder starts again about every
300 ms, leaving approximately 25 ms before its individual deadline.

## Scope and limitations

- This draft addresses even-ring FIFO startup. FIFO comparison remains request
  sequence ascending, then coder ID.
- Odd rings retain existing behavior. Odd IDs are not an independent group on
  an odd ring: for example coders 1 and 5 share a dongle when N is 5. Extending
  this design needs a separate admission strategy.
- EDF retains its existing initial registration and time-gate logic.
- The single-coder path and zero-quota path remain unchanged.
- No recurring barrier or added sleep is introduced between later cycles.
- It does not solve every possible FIFO deadline miss, prevent OS delays,
  or resolve the separate Helgrind/library warnings.
- All first requests must be submitted before grants begin. On a heavily loaded
  machine, late worker execution can delay this stage. The monitor's deadline
  and stop handling remain active throughout it.

## Validation performed

The isolated draft was built with the current Makefile, including `-g`.

- Build with `-Wall -Wextra -Werror -pthread`: passed.
- Norminette: every C/header file passed, including `fifo_start.c`.
- Existing `tests/check.py`: passed, including FIFO/EDF priority, cooldown,
  stop handling, zero values and the EDF regressions.
- Reported ten-coder command, 12 sequential repetitions: **12/12 completed**.
  Maximum consecutive compilation-start gaps were 302 or 303 ms.
- DRD with `10 2000 100 100 100 2 0 fifo`: zero errors.

The original implementation burned out in 2 of the 5 runs recorded during the
preceding diagnosis. These are observations from separate runs, not a statistical
proof that the draft can never miss a deadline.

Per-run measurements are in
[fifo_startup_results.json](fifo_startup_results.json).

## Original patch instructions (for an unpatched copy only)

The current project already contains this patch. The commands in this section
are retained only for an unpatched copy. From that copy's project root:

```sh
git apply --stat drafts/fifo_startup.patch
git apply --check drafts/fifo_startup.patch
```

When you choose to apply the draft:

```sh
git apply drafts/fifo_startup.patch
make
norminette src
make test
python3 drafts/check_fifo_startup.py
```

The regression command defaults to 12 repetitions. It reuses the log validator
from `tests/sheet_check.py` and checks completion, phase/cooldown/ownership
invariants, and compilation-start gaps below 325 ms.

The already-tested isolated build is temporarily available at
`/tmp/codexion-fifo-draft-fyy8cb0e/codexion`. To exercise that build without using the main executable:

```sh
python3 drafts/check_fifo_startup.py --binary /tmp/codexion-fifo-draft-fyy8cb0e/codexion
```

The system may remove temporary files; the patch is the persistent draft.
